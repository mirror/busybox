/* vi: set sw=4 ts=4: */
/*
 * Ask for a password
 * I use a static buffer in this function.  Plan accordingly.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>

#include "libbb.h"

/* do nothing signal handler */
static void askpass_timeout(int ATTRIBUTE_UNUSED ignore)
{
}

char *bb_askpass(int timeout, const char * prompt)
{
	static char passwd[64];

	char *ret;
	int i;
	struct sigaction sa;
	struct termios old, new;

	tcgetattr(STDIN_FILENO, &old);
	tcflush(STDIN_FILENO, TCIFLUSH);

	memset(passwd, 0, sizeof(passwd));

	fputs(prompt, stdout);
	fflush(stdout);

	tcgetattr(STDIN_FILENO, &new);
	new.c_iflag &= ~(IUCLC|IXON|IXOFF|IXANY);
	new.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|TOSTOP);
	tcsetattr(STDIN_FILENO, TCSANOW, &new);

	if (timeout) {
		sa.sa_flags = 0;
		sa.sa_handler = askpass_timeout;
		sigaction(SIGALRM, &sa, NULL);
		alarm(timeout);
	}

	ret = NULL;
	if (read(STDIN_FILENO, passwd, sizeof(passwd)-1) > 0) {
		ret = passwd;
		i = 0;
		/* Last byte is guaranteed to be 0
		   (read did not overwrite it) */
		do {
			if (passwd[i] == '\r' || passwd[i] == '\n')
				passwd[i] = '\0';
		} while (passwd[i++]);
	}

	if (timeout) {
		alarm(0);
	}

	tcsetattr(STDIN_FILENO, TCSANOW, &old);
	puts("");
	fflush(stdout);
	return ret;
}
