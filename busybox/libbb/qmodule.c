/*
   Copyright (C) 2002 Tim Riker <Tim@Rikers.org>
   everyone seems to claim it someplace. ;-)
*/

#include <errno.h>

#include "libbb.h"

int query_module(const char *name, int which, void *buf, size_t bufsize, size_t *ret);

int my_query_module(const char *name, int which, void **buf,
		size_t *bufsize, size_t *ret)
{
	int my_ret;

	my_ret = query_module(name, which, *buf, *bufsize, ret);

	if (my_ret == -1 && errno == ENOSPC) {
		*buf = xrealloc(*buf, *ret);
		*bufsize = *ret;

		my_ret = query_module(name, which, *buf, *bufsize, ret);
	}

	return my_ret;
}


