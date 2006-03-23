/* vi: set sw=4 ts=4: */
/*
 * vlock implementation for busybox
 *
 * Copyright (C) 2000 by spoon <spoon@ix.netcom.com>
 * Written by spoon <spon@ix.netcom.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* Shoutz to Michael K. Johnson <johnsonm@redhat.com>, author of the
 * original vlock.  I snagged a bunch of his code to write this
 * minimalistic vlock.
 */
/* Fixed by Erik Andersen to do passwords the tinylogin way...
 * It now works with md5, sha1, etc passwords. */

#include <stdio.h>
#include <stdlib.h>
#include <sys/vt.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "busybox.h"

static struct passwd *pw;
static struct vt_mode ovtm;
static struct termios oterm;
static int vfd;
static int o_lock_all = 0;

#ifdef CONFIG_FEATURE_SHADOWPASSWDS
static struct spwd *spw;

/* getspuid - get a shadow entry by uid */
struct spwd *getspuid(uid_t uid)
{
	struct spwd *sp;
	struct passwd *mypw;

	if ((mypw = getpwuid(getuid())) == NULL) {
		return (NULL);
	}
	setspent();
	while ((sp = getspent()) != NULL) {
		if (strcmp(mypw->pw_name, sp->sp_namp) == 0)
			break;
	}
	endspent();
	return (sp);
}
#endif

static void release_vt(int signo)
{
	if (!o_lock_all)
		ioctl(vfd, VT_RELDISP, 1);
	else
		ioctl(vfd, VT_RELDISP, 0);
}

static void acquire_vt(int signo)
{
	ioctl(vfd, VT_RELDISP, VT_ACKACQ);
}

static void restore_terminal(void)
{
	ioctl(vfd, VT_SETMODE, &ovtm);
	tcsetattr(STDIN_FILENO, TCSANOW, &oterm);
}

extern int vlock_main(int argc, char **argv)
{
	sigset_t sig;
	struct sigaction sa;
	struct vt_mode vtm;
	int times = 0;
	struct termios term;

	if (argc > 2) {
		bb_show_usage();
	}

	if (argc == 2) {
		if (strncmp(argv[1], "-a", 2)) {
			bb_show_usage();
		} else {
			o_lock_all = 1;
		}
	}

	if ((pw = getpwuid(getuid())) == NULL) {
		bb_error_msg_and_die("no password for uid %d\n", getuid());
	}
#ifdef CONFIG_FEATURE_SHADOWPASSWDS
	if ((strcmp(pw->pw_passwd, "x") == 0)
		|| (strcmp(pw->pw_passwd, "*") == 0)) {

		if ((spw = getspuid(getuid())) == NULL) {
			bb_error_msg_and_die("could not read shadow password for uid %d: %s\n",
					   getuid(), strerror(errno));
		}
		if (spw->sp_pwdp) {
			pw->pw_passwd = spw->sp_pwdp;
		}
	}
#endif							/* CONFIG_FEATURE_SHADOWPASSWDS */
	if (pw->pw_passwd[0] == '!' || pw->pw_passwd[0] == '*') {
		bb_error_msg_and_die("Account disabled for uid %d\n", getuid());
	}

	/* we no longer need root privs */
	setuid(getuid());
	setgid(getgid());

	if ((vfd = open("/dev/tty", O_RDWR)) < 0) {
		bb_error_msg_and_die("/dev/tty");
	};

	if (ioctl(vfd, VT_GETMODE, &vtm) < 0) {
		bb_error_msg_and_die("/dev/tty");
	};

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
		char *pass, *crypt_pass;
		char prompt[100];

		if (o_lock_all) {
			printf("All Virtual Consoles locked.\n");
		} else {
			printf("This Virtual Console locked.\n");
		}
		fflush(stdout);

		snprintf(prompt, 100, "%s's password: ", pw->pw_name);

		if ((pass = bb_askpass(0, prompt)) == NULL) {
			restore_terminal();
			bb_perror_msg_and_die("password");
		}

		crypt_pass = pw_encrypt(pass, pw->pw_passwd);
		if (strncmp(crypt_pass, pw->pw_passwd, sizeof(crypt_pass)) == 0) {
			memset(pass, 0, strlen(pass));
			memset(crypt_pass, 0, strlen(crypt_pass));
			restore_terminal();
			return 0;
		}
		memset(pass, 0, strlen(pass));
		memset(crypt_pass, 0, strlen(crypt_pass));

		if (isatty(STDIN_FILENO) == 0) {
			restore_terminal();
			bb_perror_msg_and_die("isatty");
		}

		sleep(++times);
		printf("Password incorrect.\n");
		if (times >= 3) {
			sleep(15);
			times = 2;
		}
	} while (1);
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
