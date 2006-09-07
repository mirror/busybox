/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include "libbb.h"

void bb_vinfo_msg(const char *s, va_list p)
{
	/* va_copy is used because it is not portable
	 * to use va_list p twice */
	va_list p2;
	va_copy(p2, p);
	if (logmode & LOGMODE_STDIO) {
		vprintf(s, p);
		putchar('\n');
	}
	if (logmode & LOGMODE_SYSLOG)
		vsyslog(LOG_INFO, s, p2);
	va_end(p2);
}
