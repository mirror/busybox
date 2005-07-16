/* vi: set sw=4 ts=4: */
/*
 * Mini dirname function.
 *
 * Copyright (C) 2001  Matt Kraai.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <string.h>
#include "libbb.h"

#if defined __GNU_LIBRARY___ < 5

/* Return a string containing the path name of the parent
 * directory of PATH.  */

char *dirname(char *path)
{
	char *s;

	/* Go to the end of the string.  */
	s = path + strlen(path) - 1;

	/* Strip off trailing /s (unless it is also the leading /).  */
	while (path < s && s[0] == '/')
		s--;

	/* Strip the last component.  */
	while (path <= s && s[0] != '/')
		s--;

	while (path < s && s[0] == '/')
		s--;

	if (s < path)
		return ".";

	s[1] = '\0';
	return path;
}

#endif
