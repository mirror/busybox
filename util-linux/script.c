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

struct globals {
	int child_pid;
	int attr_ok; /* NB: 0: ok */
	struct termios tt;
	const char *fname;
};
#define G (*ptr_to_globals)
#define child_pid (G.child_pid)
#define attr_ok   (G.attr_ok  )
#define tt        (G.tt       )
#define fname     (G.fname    )
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
	fname = "typescript"; \
} while (0)

static void done(void)
{
	if (child_pid) { /* we are parent */
		if (attr_ok == 0)
			tcsetattr(0, TCSAFLUSH, &tt);
		if (!(option_mask32 & 8)) /* not -q */
			printf("Script done, file is %s\n", fname);
	}
	exit(0);
}

#ifdef UNUSED
static void handle_sigchld(int sig)
{
	/* wait for the exited child and exit */
	while (wait_any_nohang(&sig) > 0)
		continue;
	done();
}
#endif

#if ENABLE_GETOPT_LONG
static const char getopt_longopts[] ALIGN1 =
	"append\0"  No_argument       "a"
	"command\0" Required_argument "c"
	"flush\0"   No_argument       "f"
	"quiet\0"   No_argument       "q"
	;
#endif

int script_main(int argc, char *argv[]) MAIN_EXTERNALLY_VISIBLE;
int script_main(int argc, char *argv[])
{
	int opt, pty;
	int winsz_ok;
	int mode;
	struct termios rtt;
	struct winsize win;
	char line[32];
	const char *shell;
	char shell_opt[] = "-i";
	char *shell_arg = NULL;

	INIT_G();
#if ENABLE_GETOPT_LONG
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
		shell = _PATH_BSHELL;
	}

	pty = getpty(line, sizeof(line));
	if (pty < 0) {
		bb_perror_msg_and_die("can't get pty");
	}

	/* get current stdin's tty params */
	attr_ok = tcgetattr(0, &tt);
	winsz_ok = ioctl(0, TIOCGWINSZ, (char *)&win);

	rtt = tt;
	cfmakeraw(&rtt);
	rtt.c_lflag &= ~ECHO;
	tcsetattr(0, TCSAFLUSH, &rtt);

	/* We exit as soon as child exits */
	//signal(SIGCHLD, handle_sigchld);
	signal(SIGCHLD, (void (*)(int)) done);

	child_pid = vfork();
	if (child_pid < 0) {
		bb_perror_msg_and_die("vfork");
	}

	if (child_pid) {
		/* parent */
		char buf[256];
		struct pollfd pfd[2];
		int outfd;
		int fd_count = 2;
		struct pollfd *ppfd = pfd;

		outfd = xopen(fname, mode);
		pfd[0].fd = 0;
		pfd[0].events = POLLIN;
		pfd[1].fd = pty;
		pfd[1].events = POLLIN;
		ndelay_on(pty); /* this descriptor is not shared, can do this */
		/* ndelay_on(0); - NO, stdin can be shared! */

		/* copy stdin to pty master input,
		 * copy pty master output to stdout and file */
		/* TODO: don't use full_write's, use proper write buffering */
		while (fd_count && safe_poll(ppfd, fd_count, -1) > 0) {
			if (pfd[0].revents) {
				int count = safe_read(0, buf, sizeof(buf));
				if (count <= 0) {
					/* err/eof: don't read anymore */
					pfd[0].revents = 0;
					ppfd++;
					fd_count--;
				} else {
					full_write(pty, buf, count);
				}
			}
			if (pfd[1].revents) {
				int count;
				errno = 0;
				count = safe_read(pty, buf, sizeof(buf));
				if (count <= 0 && errno != EAGAIN) {
					/* err/eof: don't read anymore */
					pfd[1].revents = 0;
					fd_count--;
				}
				if (count > 0) {
					full_write(1, buf, count);
					full_write(outfd, buf, count);
					if (opt & 4) { /* -f */
						fsync(outfd);
					}
				}
			}
		}
		done(); /* does not return */
	}

	/* child: make pty slave to be input, output, error; run shell */
	close(pty); /* close pty master */
	/* open pty slave to fd 0,1,2 */
	close(0);               
	xopen(line, O_RDWR); /* uses fd 0 */
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
