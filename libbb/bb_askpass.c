/* vi: set sw=4 ts=4: */
/*
 * Ask for a password
 * I use a static buffer in this function.  Plan accordingly.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <termios.h>

#include "libbb.h"

/* do nothing signal handler */
static void askpass_timeout(int ATTRIBUTE_UNUSED ignore)
{
}

char *bb_askpass(int timeout, const char *prompt)
{
	/* Was static char[BIGNUM] */
	enum { sizeof_passwd = 128 };
	static char *passwd;

	char *ret;
	int i;
	struct sigaction sa, oldsa;
	struct termios tio, oldtio;

	if (!passwd)
		passwd = xmalloc(sizeof_passwd);
	memset(passwd, 0, sizeof_passwd);

	tcgetattr(STDIN_FILENO, &oldtio);
	tcflush(STDIN_FILENO, TCIFLUSH);
	tio = oldtio;
	tio.c_iflag &= ~(IUCLC|IXON|IXOFF|IXANY);
	tio.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|TOSTOP);
	tcsetattr(STDIN_FILENO, TCSANOW, &tio);

	memset(&sa, 0, sizeof(sa));
	/* sa.sa_flags = 0; - no SA_RESTART! */
	/* SIGINT and SIGALRM will interrupt read below */
	sa.sa_handler = askpass_timeout;
	sigaction(SIGINT, &sa, &oldsa);
	if (timeout) {
		sigaction_set(SIGALRM, &sa);
		alarm(timeout);
	}

	fputs(prompt, stdout);
	fflush(stdout);
	ret = NULL;
	/* On timeout or Ctrl-C, read will hopefully be interrupted,
	 * and we return NULL */
	if (read(STDIN_FILENO, passwd, sizeof_passwd - 1) > 0) {
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
	sigaction_set(SIGINT, &oldsa);

	tcsetattr(STDIN_FILENO, TCSANOW, &oldtio);
	bb_putchar('\n');
	fflush(stdout);
	return ret;
}
