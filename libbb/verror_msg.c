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

smallint logmode = LOGMODE_STDIO;
const char *msg_eol = "\n";

void FAST_FUNC bb_verror_msg(const char *s, va_list p, const char* strerr)
{
	char *msg;
	int applet_len, strerr_len, msgeol_len, used;

	if (!logmode)
		return;

	if (!s) /* nomsg[_and_die] uses NULL fmt */
		s = ""; /* some libc don't like printf(NULL) */

	used = vasprintf(&msg, s, p);
	if (used < 0)
		return;

	/* This is ugly and costs +60 bytes compared to multiple
	 * fprintf's, but is guaranteed to do a single write.
	 * This is needed for e.g. httpd logging, when multiple
	 * children can produce log messages simultaneously. */

	applet_len = strlen(applet_name) + 2; /* "applet: " */
	strerr_len = strerr ? strlen(strerr) : 0;
	msgeol_len = strlen(msg_eol);
	/* +3 is for ": " before strerr and for terminating NUL */
	msg = xrealloc(msg, applet_len + used + strerr_len + msgeol_len + 3);
	/* TODO: maybe use writev instead of memmoving? Need full_writev? */
	memmove(msg + applet_len, msg, used);
	used += applet_len;
	strcpy(msg, applet_name);
	msg[applet_len - 2] = ':';
	msg[applet_len - 1] = ' ';
	if (strerr) {
		if (s[0]) { /* not perror_nomsg? */
			msg[used++] = ':';
			msg[used++] = ' ';
		}
		strcpy(&msg[used], strerr);
		used += strerr_len;
	}
	strcpy(&msg[used], msg_eol);

	if (logmode & LOGMODE_STDIO) {
		fflush(stdout);
		full_write(STDERR_FILENO, msg, used + msgeol_len);
	}
	if (logmode & LOGMODE_SYSLOG) {
		syslog(LOG_ERR, "%s", msg + applet_len);
	}
	free(msg);
}


#ifdef VERSION_WITH_WRITEV

/* Code size is approximately the same, but currently it's the only user
 * of writev in entire bbox. __libc_writev in uclibc is ~50 bytes. */

void FAST_FUNC bb_verror_msg(const char *s, va_list p, const char* strerr)
{
	int strerr_len, msgeol_len;
	struct iovec iov[3];

#define used   (iov[2].iov_len)
#define msgv   (iov[2].iov_base)
#define msgc   ((char*)(iov[2].iov_base))
#define msgptr (&(iov[2].iov_base))

	if (!logmode)
		return;

	if (!s) /* nomsg[_and_die] uses NULL fmt */
		s = ""; /* some libc don't like printf(NULL) */

	/* Prevent "derefing type-punned ptr will break aliasing rules" */
	used = vasprintf((char**)(void*)msgptr, s, p);
	if (used < 0)
		return;

	/* This is ugly and costs +60 bytes compared to multiple
	 * fprintf's, but is guaranteed to do a single write.
	 * This is needed for e.g. httpd logging, when multiple
	 * children can produce log messages simultaneously. */

	strerr_len = strerr ? strlen(strerr) : 0;
	msgeol_len = strlen(msg_eol);
	/* +3 is for ": " before strerr and for terminating NUL */
	msgv = xrealloc(msgv, used + strerr_len + msgeol_len + 3);
	if (strerr) {
		msgc[used++] = ':';
		msgc[used++] = ' ';
		strcpy(msgc + used, strerr);
		used += strerr_len;
	}
	strcpy(msgc + used, msg_eol);
	used += msgeol_len;

	if (logmode & LOGMODE_STDIO) {
		iov[0].iov_base = (char*)applet_name;
		iov[0].iov_len = strlen(applet_name);
		iov[1].iov_base = (char*)": ";
		iov[1].iov_len = 2;
		/*iov[2].iov_base = msgc;*/
		/*iov[2].iov_len = used;*/
		fflush(stdout);
		writev(2, iov, 3);
	}
	if (logmode & LOGMODE_SYSLOG) {
		syslog(LOG_ERR, "%s", msgc);
	}
	free(msgc);
}
#endif
