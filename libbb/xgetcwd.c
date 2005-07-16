/*
 * xgetcwd.c -- return current directory with unlimited length
 * Copyright (C) 1992, 1996 Free Software Foundation, Inc.
 * Written by David MacKenzie <djm@gnu.ai.mit.edu>.
 *
 * Special function for busybox written by Vladimir Oleynik <vodz@usa.net>
*/

#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/param.h>
#include "libbb.h"

/* Amount to increase buffer size by in each try. */
#define PATH_INCR 32

/* Return the current directory, newly allocated, arbitrarily long.
   Return NULL and set errno on error.
   If argument is not NULL (previous usage allocate memory), call free()
*/

char *
xgetcwd (char *cwd)
{
  char *ret;
  unsigned path_max;

  errno = 0;
  path_max = (unsigned) PATH_MAX;
  path_max += 2;                /* The getcwd docs say to do this. */

  if(cwd==0)
	cwd = xmalloc (path_max);

  errno = 0;
  while ((ret = getcwd (cwd, path_max)) == NULL && errno == ERANGE) {
      path_max += PATH_INCR;
      cwd = xrealloc (cwd, path_max);
      errno = 0;
  }

  if (ret == NULL) {
      int save_errno = errno;
      free (cwd);
      errno = save_errno;
      perror_msg("getcwd()");
      return NULL;
  }

  return cwd;
}
