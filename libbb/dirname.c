/* vi: set sw=4 ts=4: */
/*
 * dirname implementation for busybox (for libc's missing one)
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
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
 *
 */

/* Note: The previous busybox implementation did not handle NULL path
 * and also moved a pointer before path, which is not portable in C.
 * So I replaced it with my uClibc version.
 */

#include <string.h>
#include "libbb.h"

#if __GNU_LIBRARY__ < 5

extern 
char *dirname(char *path)
{
	static const char null_or_empty_or_noslash[] = ".";
	register char *s;
	register char *last;
	char *first;

	last = s = path;

	if (s != NULL) {

	LOOP:
		while (*s && (*s != '/')) ++s;
		first = s;
		while (*s == '/') ++s;
		if (*s) {
			last = first;
			goto LOOP;
		}

		if (last == path) {
			if (*last != '/') {
				goto DOT;
			}
			if ((*++last == '/') && (last[1] == 0)) {
				++last;
			}
		}
		*last = 0;
		return path;
	}
 DOT:
	return (char *) null_or_empty_or_noslash;
}

#endif
