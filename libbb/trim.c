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
#include <string.h>
#include <ctype.h>
#include "libbb.h"


void trim(char *s)
{
	size_t len = strlen(s);
	size_t lws;

	/* trim trailing whitespace */
	while (len && isspace(s[len-1])) --len;

	/* trim leading whitespace */
	if(len) {
		lws = strspn(s, " \n\r\t\v");
		memmove(s, s + lws, len -= lws);
	}
	s[len] = 0;
}
