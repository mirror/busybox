/* vi: set sw=4 ts=4: */
/*
 * simplify_path implementation for busybox
 *
 *
 * Copyright (C) 2001  Vladimir N. Oleynik <dzo@simtreas.ru>
 *
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

#include <stdlib.h>

#include "libbb.h"

static inline char *strcpy_overlap(char *dst, const char *src)
{
	char *ptr = dst;

	do *dst++ = *src; while (*src++);
	return ptr;
}

char *simplify_path(const char *path)
{
	char *s, *start, *next;

	if (path[0] == '/')
	       start = xstrdup(path);
	else {
	       s = xgetcwd(NULL);
	       start = concat_path_file(s, path);
	       free(s);
	}
	s = start;
	/* remove . and .. */
	while(*s) {
		if(*s++ == '/' && (*s == '/' || *s == 0)) {
			/* remove duplicate and trailing slashes */
			s = strcpy_overlap(s-1, s);
		}
		else if(*(s-1) == '.' && *(s-2)=='/') {
			if(*s == '/' || *s == 0) {
				/* remove . */
				s = strcpy_overlap(s-1, s); /* maybe set // */
				s--;
			} else if(*s == '.') {
				next = s+1;     /* set after ".." */
				if(*next == '/' || *next == 0) {  /* "../" */
					if((s-=2) > start)
						/* skip previous dir */
						do s--; while(*s != '/');
					/* remove previous dir */
					strcpy_overlap(s, next);
				}

			}
		}
	}
	if(start[0]==0) {
		start[0]='/';
		start[1]=0;
	}
	return start;
}
