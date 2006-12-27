/* vi: set sw=4 ts=4: */
/*
 * skip_whitespace implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <ctype.h>
#include "libbb.h"

char *skip_whitespace(const char *s)
{
	while (isspace(*s)) ++s;

	return (char *) s;
}
