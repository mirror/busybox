/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) many different people.
 * If you wrote this, please acknowledge your work.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdio.h>
#include "libbb.h"
#include "xregex.h"



void xregcomp(regex_t *preg, const char *regex, int cflags)
{
	int ret;
	if ((ret = regcomp(preg, regex, cflags)) != 0) {
		int errmsgsz = regerror(ret, preg, NULL, 0);
		char *errmsg = xmalloc(errmsgsz);
		regerror(ret, preg, errmsg, errmsgsz);
		bb_error_msg_and_die("xregcomp: %s", errmsg);
	}
}
