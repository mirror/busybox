/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) tons of folks.  Tracking down who wrote what
 * isn't something I'm going to worry about...  If you wrote something
 * here, please feel free to acknowledge your work.
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
 * Based in part on code from sash, Copyright (c) 1999 by David I. Bell 
 * Permission has been granted to redistribute this code under the GPL.
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "libbb.h"


#ifndef DMALLOC
extern void *xmalloc(size_t size)
{
	void *ptr = malloc(size);

	if (!ptr)
		error_msg_and_die(memory_exhausted);
	return ptr;
}

extern void *xrealloc(void *old, size_t size)
{
	void *ptr;

	/* SuS2 says "If size is 0 and ptr is not a null pointer, the
	 * object pointed to is freed."  Do that here, in case realloc
	 * returns a NULL, since we don't want to choke in that case. */
	if (size==0 && old) {
		free(old);
		return NULL;
	}

	ptr = realloc(old, size);
	if (!ptr)
		error_msg_and_die(memory_exhausted);
	return ptr;
}

extern void *xcalloc(size_t nmemb, size_t size)
{
	void *ptr = calloc(nmemb, size);
	if (!ptr)
		error_msg_and_die(memory_exhausted);
	return ptr;
}

extern char * xstrdup (const char *s) {
	char *t;

	if (s == NULL)
		return NULL;

	t = strdup (s);

	if (t == NULL)
		error_msg_and_die(memory_exhausted);

	return t;
}
#endif

extern char * xstrndup (const char *s, int n) {
	char *t;

	if (s == NULL)
		error_msg_and_die("xstrndup bug");

	t = xmalloc(++n);
	
	return safe_strncpy(t,s,n);
}

FILE *xfopen(const char *path, const char *mode)
{
	FILE *fp;
	if ((fp = fopen(path, mode)) == NULL)
		perror_msg_and_die("%s", path);
	return fp;
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
