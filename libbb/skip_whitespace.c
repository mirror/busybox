/* vi: set sw=4 ts=4: */
/*
 * skip_whitespace implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

char* FAST_FUNC skip_whitespace(const char *s)
{
	/* NB: isspace('\0') returns 0 */
	while (isspace(*s)) ++s;

	return (char *) s;
}

char* FAST_FUNC skip_non_whitespace(const char *s)
{
	while (*s && !isspace(*s)) ++s;

	return (char *) s;
}
