/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

int die_sleep;
jmp_buf die_jmp;

void sleep_and_die(void)
{
	if (die_sleep) {
		/* Special case: don't die, but jump */
		if (die_sleep < 0)
			longjmp(die_jmp, xfunc_error_retval);
		sleep(die_sleep);
	}
	exit(xfunc_error_retval);
}

void bb_error_msg_and_die(const char *s, ...)
{
	va_list p;

	va_start(p, s);
	bb_verror_msg(s, p, NULL);
	va_end(p);
	sleep_and_die();
}
