/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
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

#include <stdio.h>
#include <string.h>
#include "libbb.h"


char *get_last_path_component(char *path)
{
	char *s;
	register char *ptr = path;
	register char *prev = 0;

	while (*ptr)
		ptr++;
	s = ptr - 1;

	/* strip trailing slashes */
	while (s != path && *s == '/') {
		*s-- = '\0';
	}

	/* find last component */
	ptr = path;
	while (*ptr != '\0') {
		if (*ptr == '/')
			prev = ptr;
		ptr++;
	}
	s = prev;

	if (s == NULL || s[1] == '\0')
		return path;
	else
		return s+1;
}


/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
