/* vi: set sw=4 ts=4: */

/*
 * vlock implementation for busybox
 *
 * Copyright (C) 2000 by spoon <spoon@ix.netcom.com>
 * Written by spoon <spon@ix.netcom.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* Shoutz to Michael K. Johnson <johnsonm@redhat.com>, author of the
 * original vlock.  I snagged a bunch of his code to write this
 * minimalistic vlock.
 */
/* Fixed by Erik Andersen to do passwords the tinylogin way...
 * It now works with md5, sha1, etc passwords. */

#include "libbb.h"
#include <sys/vt.h>

enum { vfd = 3 };

/* static unsigned long o_lock_all; */

static void release_vt(int signo)
{
	ioctl(vfd, VT_RELDISP, (unsigned long) !option_mask32 /*!o_lock_all*/);
}

static void acquire_vt(int signo)
{
	ioctl(vfd, VT_RELDISP, VT_ACKACQ);
}

int vlock_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int vlock_main(int argc, char **argv)
{
	sigset_t sig;
	struct sigaction sa;
	struct vt_mode vtm;
	struct termios term;
	struct termios oterm;
	struct vt_mode ovtm;
	uid_t uid;
	struct passwd *pw;

	uid = getuid();
	pw = getpwuid(uid);
	if (pw == NULL)
		bb_error_msg_and_die("unknown uid %d", uid);

	if (argc > 2) {
		bb_show_usage();
	}

	/*o_lock_all = */getopt32(argv, "a");

	/* Avoid using statics - use constant fd */
	xmove_fd(xopen(CURRENT_TTY, O_RDWR), vfd);
	xioctl(vfd, VT_GETMODE, &vtm);

	/* mask a bunch of signals */
	sigprocmask(SIG_SETMASK, NULL, &sig);
	sigdelset(&sig, SIGUSR1);
	sigdelset(&sig, SIGUSR2);
	sigaddset(&sig, SIGTSTP);
	sigaddset(&sig, SIGTTIN);
	sigaddset(&sig, SIGTTOU);
	sigaddset(&sig, SIGHUP);
	sigaddset(&sig, SIGCHLD);
	sigaddset(&sig, SIGQUIT);
	sigaddset(&sig, SIGINT);

	sigemptyset(&(sa.sa_mask));
	sa.sa_flags = SA_RESTART;
	sa.sa_handler = release_vt;
	sigaction(SIGUSR1, &sa, NULL);
	sa.sa_handler = acquire_vt;
	sigaction(SIGUSR2, &sa, NULL);

	/* need to handle some signals so that we don't get killed by them */
	sa.sa_handler = SIG_IGN;
	sigaction(SIGHUP, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTSTP, &sa, NULL);

	ovtm = vtm;
	vtm.mode = VT_PROCESS;
	vtm.relsig = SIGUSR1;
	vtm.acqsig = SIGUSR2;
	ioctl(vfd, VT_SETMODE, &vtm);

	tcgetattr(STDIN_FILENO, &oterm);
	term = oterm;
	term.c_iflag &= ~BRKINT;
	term.c_iflag |= IGNBRK;
	term.c_lflag &= ~ISIG;
	term.c_lflag &= ~(ECHO | ECHOCTL);
	tcsetattr(STDIN_FILENO, TCSANOW, &term);

	do {
		printf("Virtual console%s locked by %s.\n",
				option_mask32 /*o_lock_all*/ ? "s" : "",
				pw->pw_name);
		if (correct_password(pw)) {
			break;
		}
		bb_do_delay(FAIL_DELAY);
		puts("Password incorrect");
	} while (1);

	ioctl(vfd, VT_SETMODE, &ovtm);
	tcsetattr(STDIN_FILENO, TCSANOW, &oterm);
	fflush_stdout_and_exit(0);
}
