/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "libbb.h"


#ifndef DMALLOC
#ifdef L_xmalloc
extern void *xmalloc(size_t size)
{
	void *ptr = malloc(size);
	if (ptr == NULL && size != 0)
		bb_error_msg_and_die(bb_msg_memory_exhausted);
	return ptr;
}
#endif

#ifdef L_xrealloc
extern void *xrealloc(void *ptr, size_t size)
{
	ptr = realloc(ptr, size);
	if (ptr == NULL && size != 0)
		bb_error_msg_and_die(bb_msg_memory_exhausted);
	return ptr;
}
#endif

#ifdef L_xcalloc
extern void *xcalloc(size_t nmemb, size_t size)
{
	void *ptr = calloc(nmemb, size);
	if (ptr == NULL && nmemb != 0 && size != 0)
		bb_error_msg_and_die(bb_msg_memory_exhausted);
	return ptr;
}
#endif
#endif /* DMALLOC */

#ifdef L_xstrdup
extern char * bb_xstrdup (const char *s) {
	char *t;

	if (s == NULL)
		return NULL;

	t = strdup (s);

	if (t == NULL)
		bb_error_msg_and_die(bb_msg_memory_exhausted);

	return t;
}
#endif

#ifdef L_xstrndup
extern char * bb_xstrndup (const char *s, int n) {
	char *t;

	if (s == NULL)
		bb_error_msg_and_die("bb_xstrndup bug");

	t = xmalloc(++n);

	return safe_strncpy(t,s,n);
}
#endif

#ifdef L_xfopen
FILE *bb_xfopen(const char *path, const char *mode)
{
	FILE *fp;
	if ((fp = fopen(path, mode)) == NULL)
		bb_perror_msg_and_die("%s", path);
	return fp;
}
#endif

#ifdef L_xopen
extern int bb_xopen(const char *pathname, int flags)
{
	int ret;

	ret = open(pathname, flags, 0777);
	if (ret == -1) {
		bb_perror_msg_and_die("%s", pathname);
	}
	return ret;
}
#endif

#ifdef L_xread
extern ssize_t bb_xread(int fd, void *buf, size_t count)
{
	ssize_t size;

	size = read(fd, buf, count);
	if (size == -1) {
		bb_perror_msg_and_die("Read error");
	}
	return(size);
}
#endif

#ifdef L_xread_all
extern void bb_xread_all(int fd, void *buf, size_t count)
{
	ssize_t size;

	while (count) {
		if ((size = bb_xread(fd, buf, count)) == 0) {	/* EOF */
			bb_error_msg_and_die("Short read");
		}
		count -= size;
		buf = ((char *) buf) + size;
	}
	return;
}
#endif

#ifdef L_xread_char
extern unsigned char bb_xread_char(int fd)
{
	char tmp;

	bb_xread_all(fd, &tmp, 1);

	return(tmp);
}
#endif

#ifdef L_xferror
extern void bb_xferror(FILE *fp, const char *fn)
{
	if (ferror(fp)) {
		bb_error_msg_and_die("%s", fn);
	}
}
#endif

#ifdef L_xferror_stdout
extern void bb_xferror_stdout(void)
{
	bb_xferror(stdout, bb_msg_standard_output);
}
#endif

#ifdef L_xfflush_stdout
extern void bb_xfflush_stdout(void)
{
	if (fflush(stdout)) {
		bb_perror_msg_and_die(bb_msg_standard_output);
	}
}
#endif

#ifdef L_strlen
/* Stupid gcc always includes its own builtin strlen()... */
#undef strlen
size_t bb_strlen(const char *string)
{
	    return(strlen(string));
}
#endif

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
