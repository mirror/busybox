/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2003 Erik Andersen <andersen@codepoet.org>
 */


#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <assert.h>
#include <ctype.h>

#include "busybox.h"

extern long bb_xgetlarg(const char *arg, int base, long lower, long upper)
{
	long result;
	char *endptr;
	int errno_save = errno;

	assert(arg!=NULL);

	/* Don't allow leading whitespace. */
	if ((isspace)(*arg)) {	/* Use an actual funciton call for minimal size. */
		bb_show_usage();
	}

	errno = 0;
	result = strtol(arg, &endptr, base);
	if (errno != 0 || *endptr!='\0' || endptr==arg || result < lower || result > upper)
		bb_show_usage();
	errno = errno_save;
	return result;
}
