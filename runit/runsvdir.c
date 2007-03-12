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

/* Busyboxed by Denis Vlasenko <vda.linux@googlemail.com> */
/* TODO: depends on runit_lib.c - review and reduce/eliminate */

#include <sys/poll.h>
#include <sys/file.h>
#include "busybox.h"
#include "runit_lib.h"

#define MAXSERVICES 1000

static char *svdir;
static unsigned long dev;
static unsigned long ino;
static struct service {
	unsigned long dev;
	unsigned long ino;
	int pid;
	int isgone;
} *sv;
static int svnum;
static int check = 1;
static char *rplog;
static int rploglen;
static int logpipe[2];
static iopause_fd io[1];
static struct taia stamplog;
static int exitsoon;
static int pgrp;

#define usage() bb_show_usage()
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

static void s_term(int sig_no)
{
	exitsoon = 1;
}
static void s_hangup(int sig_no)
{
	exitsoon = 2;
}

static void runsv(int no, const char *name)
{
	int pid = fork();

	if (pid == -1) {
		warn2_cannot("fork for ", name);
		return;
	}
	if (pid == 0) {
		/* child */
		char *prog[3];

		prog[0] = (char*)"runsv";
		prog[1] = (char*)name;
		prog[2] = NULL;
		sig_uncatch(SIGHUP);
		sig_uncatch(SIGTERM);
		if (pgrp) setsid();
		BB_EXECVP(prog[0], prog);
		//pathexec_run(*prog, prog, (char* const*)environ);
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
	errno = 0;
	while ((d = readdir(dir))) {
		if (d->d_name[0] == '.') continue;
		if (stat(d->d_name, &s) == -1) {
			warn2_cannot("stat ", d->d_name);
			errno = 0;
			continue;
		}
		if (!S_ISDIR(s.st_mode)) continue;
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
			//sv[i].pid = 0;
			//sv[i].isgone = 0;
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
	if (pipe(logpipe) == -1) {
		warnx("cannot create pipe for log");
		return -1;
	}
	coe(logpipe[1]);
	coe(logpipe[0]);
	ndelay_on(logpipe[0]);
	ndelay_on(logpipe[1]);
	if (fd_copy(2, logpipe[1]) == -1) {
		warnx("cannot set filedescriptor for log");
		return -1;
	}
	io[0].fd = logpipe[0];
	io[0].events = IOPAUSE_READ;
	taia_now(&stamplog);
	return 1;
}

int runsvdir_main(int argc, char **argv);
int runsvdir_main(int argc, char **argv)
{
	struct stat s;
	time_t mtime = 0;
	int wstat;
	int curdir;
	int pid;
	struct taia deadline;
	struct taia now;
	struct taia stampcheck;
	char ch;
	int i;

	argv++;
	if (!argv || !*argv) usage();
	if (**argv == '-') {
		switch (*(*argv + 1)) {
		case 'P': pgrp = 1;
		case '-': ++argv;
		}
		if (!argv || !*argv) usage();
	}

	sig_catch(SIGTERM, s_term);
	sig_catch(SIGHUP, s_hangup);
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
	coe(curdir);

	taia_now(&stampcheck);

	for (;;) {
		/* collect children */
		for (;;) {
			pid = wait_nohang(&wstat);
			if (pid <= 0) break;
			for (i = 0; i < svnum; i++) {
				if (pid == sv[i].pid) {
					/* runsv has gone */
					sv[i].pid = 0;
					check = 1;
					break;
				}
			}
		}

		taia_now(&now);
		if (now.sec.x < (stampcheck.sec.x - 3)) {
			/* time warp */
			warnx("time warp: resetting time stamp");
			taia_now(&stampcheck);
			taia_now(&now);
			if (rplog) taia_now(&stamplog);
		}
		if (taia_less(&now, &stampcheck) == 0) {
			/* wait at least a second */
			taia_uint(&deadline, 1);
			taia_add(&stampcheck, &now, &deadline);

			if (stat(svdir, &s) != -1) {
				if (check || s.st_mtime != mtime
				 || s.st_ino != ino || s.st_dev != dev
				) {
					/* svdir modified */
					if (chdir(svdir) != -1) {
						mtime = s.st_mtime;
						dev = s.st_dev;
						ino = s.st_ino;
						check = 0;
						if (now.sec.x <= (4611686018427387914ULL + (uint64_t)mtime))
							sleep(1);
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
			if (taia_less(&now, &stamplog) == 0) {
				write(logpipe[1], ".", 1);
				taia_uint(&deadline, 900);
				taia_add(&stamplog, &now, &deadline);
			}
		}
		taia_uint(&deadline, check ? 1 : 5);
		taia_add(&deadline, &now, &deadline);

		sig_block(SIGCHLD);
		if (rplog)
			iopause(io, 1, &deadline, &now);
		else
			iopause(0, 0, &deadline, &now);
		sig_unblock(SIGCHLD);

		if (rplog && (io[0].revents | IOPAUSE_READ))
			while (read(logpipe[0], &ch, 1) > 0)
				if (ch) {
					for (i = 6; i < rploglen; i++)
						rplog[i-1] = rplog[i];
					rplog[rploglen-1] = ch;
				}

		switch (exitsoon) {
		case 1:
			_exit(0);
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
