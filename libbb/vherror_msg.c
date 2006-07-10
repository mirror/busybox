/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdarg.h>
#include <netdb.h>
#include <stdio.h>

#include "libbb.h"


void bb_vherror_msg(const char *s, va_list p)
{
	if(s == 0)
		s = "";
	bb_verror_msg(s, p);
	if (*s)
		fputs(": ", stderr);
	herror("");
}
