/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include <syslog.h>

int logmode = LOGMODE_STDIO;
const char *msg_eol = "\n";

void bb_verror_msg(const char *s, va_list p, const char* strerr)
{
	/* va_copy is used because it is not portable
	 * to use va_list p twice */
	va_list p2;
	va_copy(p2, p);

	if (logmode & LOGMODE_STDIO) {
		fflush(stdout);
		fprintf(stderr, "%s: ", applet_name);
		vfprintf(stderr, s, p);
		if (!strerr)
			fputs(msg_eol, stderr);
		else
			fprintf(stderr, "%s%s%s",
					s ? ": " : "",
					strerr, msg_eol);
	}
	if (ENABLE_FEATURE_SYSLOG && (logmode & LOGMODE_SYSLOG)) {
		if (!strerr)
			vsyslog(LOG_ERR, s, p2);
		else  {
			char *msg;
			if (vasprintf(&msg, s, p2) < 0)
				bb_error_msg_and_die(bb_msg_memory_exhausted);
			syslog(LOG_ERR, "%s: %s", msg, strerr);
			free(msg);
		}
	}
	va_end(p2);
}
