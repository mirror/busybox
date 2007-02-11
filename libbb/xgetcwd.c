/* vi: set sw=4 ts=4: */
/*
 * xgetcwd.c -- return current directory with unlimited length
 * Copyright (C) 1992, 1996 Free Software Foundation, Inc.
 * Written by David MacKenzie <djm@gnu.ai.mit.edu>.
 *
 * Special function for busybox written by Vladimir Oleynik <dzo@simtreas.ru>
*/

#include "libbb.h"

/* Amount to increase buffer size by in each try. */
#define PATH_INCR 32

/* Return the current directory, newly allocated, arbitrarily long.
   Return NULL and set errno on error.
   If argument is not NULL (previous usage allocate memory), call free()
*/

char *
xrealloc_getcwd_or_warn(char *cwd)
{
	char *ret;
	unsigned path_max;

	path_max = (unsigned) PATH_MAX;
	path_max += 2;                /* The getcwd docs say to do this. */

	if (cwd == NULL)
		cwd = xmalloc(path_max);

	while ((ret = getcwd(cwd, path_max)) == NULL && errno == ERANGE) {
		path_max += PATH_INCR;
		cwd = xrealloc(cwd, path_max);
	}

	if (ret == NULL) {
		free(cwd);
		bb_perror_msg("getcwd");
		return NULL;
	}

	return cwd;
}
