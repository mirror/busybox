/* vi: set sw=4 ts=4: */
/*
 * busybox library eXtended function
 *
 * Copyright (C) 2001 Larry Doolittle, <ldoolitt@recycle.lbl.gov>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
#include "libbb.h"

/* Find out if the last character of a string matches the one given */
char* FAST_FUNC last_char_is(const char *s, int c)
{
	if (s) {
		size_t sz = strlen(s);
		/* Don't underrun the buffer if the string length is 0 */
		if (sz != 0) {
			s += sz - 1;
			if ((unsigned char)*s == c)
				return (char*)s;
		}
	}
	return NULL;
}
