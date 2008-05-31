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

struct service {
	dev_t dev;
	ino_t ino;
	pid_t pid;
	smallint isgone;
};

struct globals {
	struct service *sv;
	char *svdir;
	char *rplog;
	int svnum;
	int rploglen;
	struct fd_pair logpipe;
	struct pollfd pfd[1];
	unsigned stamplog;
	smallint check; /* = 1; */
	smallint exitsoon;
	smallint set_pgrp;
};
#define G (*(struct globals*)&bb_common_bufsiz1)
#define sv        (G.sv        )
#define svdir     (G.svdir     )
#define rplog     (G.rplog     )
#define svnum     (G.svnum     )
#define rploglen  (G.rploglen  )
#define logpipe   (G.logpipe   )
#define pfd       (G.pfd       )
#define stamplog  (G.stamplog  )
#define check     (G.check     )
#define exitsoon  (G.exitsoon  )
#define set_pgrp  (G.set_pgrp  )
#define INIT_G() do { \
	check = 1; \
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
static void warnx(const char *m1)
{
	warn3x(m1, "", "");
}

static void s_term(int sig_no ATTRIBUTE_UNUSED)
{
	exitsoon = 1;
}
static void s_hangup(int sig_no ATTRIBUTE_UNUSED)
{
	exitsoon = 2;
}

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

static void runsvdir(void)
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
			errno = 0;
			continue;
		}
		if (!S_ISDIR(s.st_mode))
			continue;
		for (i = 0; i < svnum; i++) {
			if ((sv[i].ino == s.st_ino) && (sv[i].dev == s.st_dev)) {
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
				warn3x("cannot start runsv ", d->d_name,
						" too many services");
				continue;
			}
			sv = svnew;
			svnum++;
			memset(&sv[i], 0, sizeof(sv[i]));
			sv[i].ino = s.st_ino;
			sv[i].dev = s.st_dev;
			/*sv[i].pid = 0;*/
			/*sv[i].isgone = 0;*/
			runsv(i, d->d_name);
			check = 1;
		}
	}
	if (errno) {
		warn2_cannot("read directory ", svdir);
		closedir(dir);
		check = 1;
		return;
	}
	closedir(dir);

	/* SIGTERM removed runsv's */
	for (i = 0; i < svnum; i++) {
		if (!sv[i].isgone)
			continue;
		if (sv[i].pid)
			kill(sv[i].pid, SIGTERM);
		sv[i] = sv[--svnum];
/* BUG? we deleted sv[i] by copying over sv[last], but we will not check this newly-copied one! */
		check = 1;
	}
}

static int setup_log(void)
{
	rploglen = strlen(rplog);
	if (rploglen < 7) {
		warnx("log must have at least seven characters");
		return 0;
	}
	if (piped_pair(logpipe)) {
		warnx("cannot create pipe for log");
		return -1;
	}
	close_on_exec_on(logpipe.rd);
	close_on_exec_on(logpipe.wr);
	ndelay_on(logpipe.rd);
	ndelay_on(logpipe.wr);
	if (dup2(logpipe.wr, 2) == -1) {
		warnx("cannot set filedescriptor for log");
		return -1;
	}
	pfd[0].fd = logpipe.rd;
	pfd[0].events = POLLIN;
	stamplog = monotonic_sec();
	return 1;
}

int runsvdir_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int runsvdir_main(int argc ATTRIBUTE_UNUSED, char **argv)
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
	char ch;
	int i;

	INIT_G();

	argv++;
	if (!*argv)
		bb_show_usage();
	if (argv[0][0] == '-') {
		switch (argv[0][1]) {
		case 'P': set_pgrp = 1;
		case '-': ++argv;
		}
		if (!*argv)
			bb_show_usage();
	}

	bb_signals_recursive(1 << SIGTERM, s_term);
	bb_signals_recursive(1 << SIGHUP, s_hangup);
	svdir = *argv++;
	if (argv && *argv) {
		rplog = *argv;
		if (setup_log() != 1) {
			rplog = 0;
			warnx("log service disabled");
		}
	}
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
					check = 1;
					break;
				}
			}
		}

		now = monotonic_sec();
		if ((int)(now - stampcheck) >= 0) {
			/* wait at least a second */
			stampcheck = now + 1;

			if (stat(svdir, &s) != -1) {
				if (check || s.st_mtime != last_mtime
				 || s.st_ino != last_ino || s.st_dev != last_dev
				) {
					/* svdir modified */
					if (chdir(svdir) != -1) {
						last_mtime = s.st_mtime;
						last_dev = s.st_dev;
						last_ino = s.st_ino;
						check = 0;
						//if (now <= mtime)
						//	sleep(1);
						runsvdir();
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

		if (rplog) {
			if ((int)(now - stamplog) >= 0) {
				write(logpipe.wr, ".", 1);
				stamplog = now + 900;
			}
		}

		pfd[0].revents = 0;
		sig_block(SIGCHLD);
		deadline = (check ? 1 : 5);
		if (rplog)
			poll(pfd, 1, deadline*1000);
		else
			sleep(deadline);
		sig_unblock(SIGCHLD);

		if (pfd[0].revents & POLLIN) {
			while (read(logpipe.rd, &ch, 1) > 0) {
				if (ch) {
					for (i = 6; i < rploglen; i++)
						rplog[i-1] = rplog[i];
					rplog[rploglen-1] = ch;
				}
			}
		}

		switch (exitsoon) {
		case 1:
			_exit(EXIT_SUCCESS);
		case 2:
			for (i = 0; i < svnum; i++)
				if (sv[i].pid)
					kill(sv[i].pid, SIGTERM);
			_exit(111);
		}
	}
	/* not reached */
	return 0;
}
