/* vi: set sw=4 ts=4: */
/*
 * script implementation for busybox
 *
 * pascal.bellard@ads-lu.com
 *
 * Based on code from util-linux v 2.12r
 * Copyright (c) 1980
 *	The Regents of the University of California.  All rights reserved.
 *
 * Licensed under GPLv2 or later, see file License in this tarball for details.
 */

#include "libbb.h"

static smallint fd_count = 2;

static void handle_sigchld(int sig ATTRIBUTE_UNUSED)
{
	fd_count = 0;
}

int script_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int script_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	int opt;
	int mode;
	int child_pid;
	int attr_ok; /* NB: 0: ok */
	int winsz_ok;
	int pty;
	char pty_line[GETPTY_BUFSIZE];
	struct termios tt, rtt;
	struct winsize win;
	const char *fname = "typescript";
	const char *shell;
	char shell_opt[] = "-i";
	char *shell_arg = NULL;

#if ENABLE_GETOPT_LONG
	static const char getopt_longopts[] ALIGN1 =
		"append\0"  No_argument       "a"
		"command\0" Required_argument "c"
		"flush\0"   No_argument       "f"
		"quiet\0"   No_argument       "q"
		;

	applet_long_options = getopt_longopts;
#endif
	opt_complementary = "?1"; /* max one arg */
	opt = getopt32(argv, "ac:fq", &shell_arg);
	//argc -= optind;
	argv += optind;
	if (argv[0]) {
		fname = argv[0];
	}
	mode = O_CREAT|O_TRUNC|O_WRONLY;
	if (opt & 1) {
		mode = O_CREAT|O_APPEND|O_WRONLY;
	}
	if (opt & 2) {
		shell_opt[1] = 'c';
	}
	if (!(opt & 8)) { /* not -q */
		printf("Script started, file is %s\n", fname);
	}
	shell = getenv("SHELL");
	if (shell == NULL) {
		shell = DEFAULT_SHELL;
	}

	pty = xgetpty(pty_line);

	/* get current stdin's tty params */
	attr_ok = tcgetattr(0, &tt);
	winsz_ok = ioctl(0, TIOCGWINSZ, (char *)&win);

	rtt = tt;
	cfmakeraw(&rtt);
	rtt.c_lflag &= ~ECHO;
	tcsetattr(0, TCSAFLUSH, &rtt);

	/* "script" from util-linux exits when child exits,
	 * we wouldn't wait for EOF from slave pty
	 * (output may be produced by grandchildren of child) */
	signal(SIGCHLD, handle_sigchld);

	/* TODO: SIGWINCH? pass window size changes down to slave? */

	child_pid = vfork();
	if (child_pid < 0) {
		bb_perror_msg_and_die("vfork");
	}

	if (child_pid) {
		/* parent */
#define buf bb_common_bufsiz1
		struct pollfd pfd[2];
		int outfd, count, loop;

		outfd = xopen(fname, mode);
		pfd[0].fd = pty;
		pfd[0].events = POLLIN;
		pfd[1].fd = 0;
		pfd[1].events = POLLIN;
		ndelay_on(pty); /* this descriptor is not shared, can do this */
		/* ndelay_on(0); - NO, stdin can be shared! Pity :( */

		/* copy stdin to pty master input,
		 * copy pty master output to stdout and file */
		/* TODO: don't use full_write's, use proper write buffering */
		while (fd_count) {
			/* not safe_poll! we want SIGCHLD to EINTR poll */
			if (poll(pfd, fd_count, -1) < 0 && errno != EINTR) {
				/* If child exits too quickly, we may get EIO:
				 * for example, try "script -c true" */
				break;
			}
			if (pfd[0].revents) {
				errno = 0;
				count = safe_read(pty, buf, sizeof(buf));
				if (count <= 0 && errno != EAGAIN) {
					/* err/eof from pty: exit */
					goto restore;
				}
				if (count > 0) {
					full_write(STDOUT_FILENO, buf, count);
					full_write(outfd, buf, count);
					if (opt & 4) { /* -f */
						fsync(outfd);
					}
				}
			}
			if (pfd[1].revents) {
				count = safe_read(STDIN_FILENO, buf, sizeof(buf));
				if (count <= 0) {
					/* err/eof from stdin: don't read stdin anymore */
					pfd[1].revents = 0;
					fd_count--;
				} else {
					full_write(pty, buf, count);
				}
			}
		}
		/* If loop was exited because SIGCHLD handler set fd_count to 0,
		 * there still can be some buffered output. But not loop forever:
		 * we won't pump orphaned grandchildren's output indefinitely.
		 * Testcase: running this in script:
		 *      exec dd if=/dev/zero bs=1M count=1
		 * must have "1+0 records in, 1+0 records out" captured too.
		 * (util-linux's script doesn't do this. buggy :) */
		loop = 999;
		/* pty is in O_NONBLOCK mode, we exit as soon as buffer is empty */
		while (--loop && (count = safe_read(pty, buf, sizeof(buf))) > 0) {
			full_write(STDOUT_FILENO, buf, count);
			full_write(outfd, buf, count);
		}
 restore:
		if (attr_ok == 0)
			tcsetattr(0, TCSAFLUSH, &tt);
		if (!(opt & 8)) /* not -q */
			printf("Script done, file is %s\n", fname);
		return EXIT_SUCCESS;
	}

	/* child: make pty slave to be input, output, error; run shell */
	close(pty); /* close pty master */
	/* open pty slave to fd 0,1,2 */
	close(0);
	xopen(pty_line, O_RDWR); /* uses fd 0 */
	xdup2(0, 1);
	xdup2(0, 2);
	/* copy our original stdin tty's parameters to pty */
	if (attr_ok == 0)
		tcsetattr(0, TCSAFLUSH, &tt);
	if (winsz_ok == 0)
		ioctl(0, TIOCSWINSZ, (char *)&win);
	/* set pty as a controlling tty */
	setsid();
	ioctl(0, TIOCSCTTY, 0 /* 0: don't forcibly steal */);

	/* signal(SIGCHLD, SIG_DFL); - exec does this for us */
	execl(shell, shell, shell_opt, shell_arg, NULL);
	bb_simple_perror_msg_and_die(shell);
}
