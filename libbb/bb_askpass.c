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
#define PWD_BUFFER_SIZE 256


/* do nothing signal handler */
static void askpass_timeout(int ATTRIBUTE_UNUSED ignore)
{
}

char *bb_askpass(int timeout, const char * prompt)
{
	char *ret;
	int i, size;
	struct sigaction sa;
	struct termios old, new;
	static char passwd[PWD_BUFFER_SIZE];

	tcgetattr(STDIN_FILENO, &old);
	tcflush(STDIN_FILENO, TCIFLUSH);

	size = sizeof(passwd);
	ret = passwd;
	memset(passwd, 0, size);

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

	if (read(STDIN_FILENO, passwd, size-1) <= 0) {
		ret = NULL;
	} else {
		for(i = 0; i < size && passwd[i]; i++) {
			if (passwd[i]== '\r' || passwd[i] == '\n') {
				passwd[i]= 0;
				break;
			}
		}
	}

	if (timeout) {
		alarm(0);
	}

	tcsetattr(STDIN_FILENO, TCSANOW, &old);
	fputs("\n", stdout);
	fflush(stdout);
	return ret;
}

