/* vi: set sw=4 ts=4: */
/*
 * Ask for a password
 * I use a static buffer in this function.  Plan accordingly.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
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
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>
#define PWD_BUFFER_SIZE 256


/* do nothing signal handler */
static void askpass_timeout(int ignore)
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

