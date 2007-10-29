/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) many different people.
 * If you wrote this, please acknowledge your work.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

void trim(char *s)
{
	size_t len = strlen(s);
	size_t lws;

	/* trim trailing whitespace */
	while (len && isspace(s[len-1]))
		--len;

	/* trim leading whitespace */
	if (len) {
		lws = strspn(s, " \n\r\t\v");
		if (lws) {
			len -= lws;
			memmove(s, s + lws, len);
		}
	}
	s[len] = '\0';
}
