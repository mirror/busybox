/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2003 Erik Andersen <andersee@debian.org>
 */


#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>

#include "busybox.h"

extern long bb_xgetlarg(char *arg, int base, long lower, long upper)
{
	long result;
	char *endptr;
	int errno_save = errno;

	assert(arg!=NULL);
	errno = 0;
	result = strtol(arg, &endptr, base);
	if (errno != 0 || *endptr!='\0' || result < lower || result > upper)
		show_usage();
	errno = errno_save;
	return result;
}
