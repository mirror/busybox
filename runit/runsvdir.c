/*
Copyright (c) 2001-2006, Gerrit Pape
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. The name of the author may not be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO
EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/* Busyboxed by Denys Vlasenko <vda.linux@googlemail.com> */
/* TODO: depends on runit_lib.c - review and reduce/eliminate */

#include <sys/poll.h>
#include <sys/file.h>
#include "libbb.h"
#include "runit_lib.h"

#define MAXSERVICES 1000

/* Should be not needed - all dirs are on same FS, right? */
#define CHECK_DEVNO_TOO 0

struct service {
#if CHECK_DEVNO_TOO
	dev_t dev;
#endif
	ino_t ino;
	pid_t pid;
	smallint isgone;
};

struct globals {
	struct service *sv;
	char *svdir;
	int svnum;
#if ENABLE_FEATURE_RUNSVDIR_LOG
	char *rplog;
	int rploglen;
	struct fd_pair logpipe;
	struct pollfd pfd[1];
	unsigned stamplog;
#endif
	smallint need_rescan; /* = 1; */
	smallint set_pgrp;
};
#define G (*(struct globals*)&bb_common_bufsiz1)
#define sv          (G.sv          )
#define svdir       (G.svdir       )
#define svnum       (G.svnum       )
#define rplog       (G.rplog       )
#define rploglen    (G.rploglen    )
#define logpipe     (G.logpipe     )
#define pfd         (G.pfd         )
#define stamplog    (G.stamplog    )
#define need_rescan (G.need_rescan )
#define set_pgrp    (G.set_pgrp    )
#define INIT_G() do { \
	need_rescan = 1; \
} while (0)

static void fatal2_cannot(const char *m1, const char *m2)
{
	bb_perror_msg_and_die("%s: fatal: cannot %s%s", svdir, m1, m2);
	/* was exiting 100 */
}
static void warn3x(const char *m1, const char *m2, const char *m3)
{
	bb_error_msg("%s: warning: %s%s%s", svdir, m1, m2, m3);
}
static void warn2_cannot(const char *m1, const char *m2)
{
	warn3x("cannot ", m1, m2);
}
#if ENABLE_FEATURE_RUNSVDIR_LOG
static void warnx(const char *m1)
{
	warn3x(m1, "", "");
}
#endif

static void runsv(int no, const char *name)
{
	pid_t pid;
	char *prog[3];

	prog[0] = (char*)"runsv";
	prog[1] = (char*)name;
	prog[2] = NULL;

	pid = vfork();

	if (pid == -1) {
		warn2_cannot("vfork", "");
		return;
	}
	if (pid == 0) {
		/* child */
		if (set_pgrp)
			setsid();
		bb_signals(0
			+ (1 << SIGHUP)
			+ (1 << SIGTERM)
			, SIG_DFL);
		execvp(prog[0], prog);
		fatal2_cannot("start runsv ", name);
	}
	sv[no].pid = pid;
}

static void do_rescan(void)
{
	DIR *dir;
	direntry *d;
	int i;
	struct stat s;

	dir = opendir(".");
	if (!dir) {
		warn2_cannot("open directory ", svdir);
		return;
	}
	for (i = 0; i < svnum; i++)
		sv[i].isgone = 1;

	while (1) {
		errno = 0;
		d = readdir(dir);
		if (!d)
			break;
		if (d->d_name[0] == '.')
			continue;
		if (stat(d->d_name, &s) == -1) {
			warn2_cannot("stat ", d->d_name);
			continue;
		}
		if (!S_ISDIR(s.st_mode))
			continue;
		for (i = 0; i < svnum; i++) {
			if ((sv[i].ino == s.st_ino)
#if CHECK_DEVNO_TOO
			 && (sv[i].dev == s.st_dev)
#endif
			) {
				sv[i].isgone = 0;
				if (!sv[i].pid)
					runsv(i, d->d_name);
				break;
			}
		}
		if (i == svnum) {
			/* new service */
			struct service *svnew = realloc(sv, (i+1) * sizeof(*sv));
			if (!svnew) {
				warn2_cannot("start runsv ", d->d_name);
				continue;
			}
			sv = svnew;
			svnum++;
			memset(&sv[i], 0, sizeof(sv[i]));
			sv[i].ino = s.st_ino;
#if CHECK_DEVNO_TOO
			sv[i].dev = s.st_dev;
#endif
			/*sv[i].pid = 0;*/
			/*sv[i].isgone = 0;*/
			runsv(i, d->d_name);
			need_rescan = 1;
		}
	}
	i = errno;
	closedir(dir);
	if (i) {
		warn2_cannot("read directory ", svdir);
		need_rescan = 1;
		return;
	}

	/* Send SIGTERM to runsv whose directories were not found (removed) */
	for (i = 0; i < svnum; i++) {
		if (!sv[i].isgone)
			continue;
		if (sv[i].pid)
			kill(sv[i].pid, SIGTERM);
		svnum--;
		sv[i] = sv[svnum];
		i--; /* so that we don't skip new sv[i] (bug was here!) */
		need_rescan = 1;
	}
}

int runsvdir_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int runsvdir_main(int argc UNUSED_PARAM, char **argv)
{
	struct stat s;
	dev_t last_dev = last_dev; /* for gcc */
	ino_t last_ino = last_ino; /* for gcc */
	time_t last_mtime = 0;
	int wstat;
	int curdir;
	int pid;
	unsigned deadline;
	unsigned now;
	unsigned stampcheck;
	int i;

	INIT_G();

	opt_complementary = "-1";
	set_pgrp = getopt32(argv, "P");
	argv += optind;

	bb_signals_recursive((1 << SIGTERM) | (1 << SIGHUP), record_signo);
	svdir = *argv++;

#if ENABLE_FEATURE_RUNSVDIR_LOG
	/* setup log */
	if (*argv) {
		rplog = *argv;
		rploglen = strlen(rplog);
		if (rploglen < 7) {
			warnx("log must have at least seven characters");
		} else if (piped_pair(logpipe)) {
			warnx("cannot create pipe for log");
		} else {
			close_on_exec_on(logpipe.rd);
			close_on_exec_on(logpipe.wr);
			ndelay_on(logpipe.rd);
			ndelay_on(logpipe.wr);
			if (dup2(logpipe.wr, 2) == -1) {
				warnx("cannot set filedescriptor for log");
			} else {
				pfd[0].fd = logpipe.rd;
				pfd[0].events = POLLIN;
				stamplog = monotonic_sec();
				goto run;
			}
		}
		rplog = NULL;
		warnx("log service disabled");
	}
run:
#endif
	curdir = open_read(".");
	if (curdir == -1)
		fatal2_cannot("open current directory", "");
	close_on_exec_on(curdir);

	stampcheck = monotonic_sec();

	for (;;) {
		/* collect children */
		for (;;) {
			pid = wait_any_nohang(&wstat);
			if (pid <= 0)
				break;
			for (i = 0; i < svnum; i++) {
				if (pid == sv[i].pid) {
					/* runsv has gone */
					sv[i].pid = 0;
					need_rescan = 1;
					break;
				}
			}
		}

		now = monotonic_sec();
		if ((int)(now - stampcheck) >= 0) {
			/* wait at least a second */
			stampcheck = now + 1;

			if (stat(svdir, &s) != -1) {
				if (need_rescan || s.st_mtime != last_mtime
				 || s.st_ino != last_ino || s.st_dev != last_dev
				) {
					/* svdir modified */
					if (chdir(svdir) != -1) {
						last_mtime = s.st_mtime;
						last_dev = s.st_dev;
						last_ino = s.st_ino;
						need_rescan = 0;
						//if (now <= mtime)
						//	sleep(1);
						do_rescan();
						while (fchdir(curdir) == -1) {
							warn2_cannot("change directory, pausing", "");
							sleep(5);
						}
					} else
						warn2_cannot("change directory to ", svdir);
				}
			} else
				warn2_cannot("stat ", svdir);
		}

#if ENABLE_FEATURE_RUNSVDIR_LOG
		if (rplog) {
			if ((int)(now - stamplog) >= 0) {
				write(logpipe.wr, ".", 1);
				stamplog = now + 900;
			}
		}
		pfd[0].revents = 0;
#endif
		sig_block(SIGCHLD);
		deadline = (need_rescan ? 1 : 5);
#if ENABLE_FEATURE_RUNSVDIR_LOG
		if (rplog)
			poll(pfd, 1, deadline*1000);
		else
#endif
			sleep(deadline);
		sig_unblock(SIGCHLD);

#if ENABLE_FEATURE_RUNSVDIR_LOG
		if (pfd[0].revents & POLLIN) {
			char ch;
			while (read(logpipe.rd, &ch, 1) > 0) {
				if (ch) {
					for (i = 6; i < rploglen; i++)
						rplog[i-1] = rplog[i];
					rplog[rploglen-1] = ch;
				}
			}
		}
#endif
		switch (bb_got_signal) {
		case SIGHUP:
			for (i = 0; i < svnum; i++)
				if (sv[i].pid)
					kill(sv[i].pid, SIGTERM);
			// N.B. fall through
		case SIGTERM:
			_exit((SIGHUP == bb_got_signal) ? 111 : EXIT_SUCCESS);
		}
	}
	/* not reached */
	return 0;
}
