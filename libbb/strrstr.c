/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2008 Bernhard Fischer
 *
 * Licensed under GPLv2 or later, see file License in this tarball for details.
 */

#include "libbb.h"

/* reverse strstr() */
char* strrstr(const char *haystack, const char *needle)
{
	char *tmp = strrchr(haystack, *needle);
	if (tmp == NULL || strcmp(tmp, needle) != 0)
		return NULL;
	return tmp;
}

