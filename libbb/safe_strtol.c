/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include "libbb.h"

#ifdef L_safe_strtoi
int safe_strtoi(char *arg, int* value)
{
	int error;
	long lvalue = *value;
	error = safe_strtol(arg, &lvalue);
	*value = (int) lvalue;
	return error;
}
#endif

#ifdef L_safe_strtod
int safe_strtod(char *arg, double* value)
{
	char *endptr;
	int errno_save = errno;

	assert(arg!=NULL);
	errno = 0;
	*value = strtod(arg, &endptr);
	if (errno != 0 || *endptr!='\0' || endptr==arg) {
		return 1;
	}
	errno = errno_save;
	return 0;
}
#endif

#ifdef L_safe_strtol
int safe_strtol(char *arg, long* value)
{
	char *endptr;
	int errno_save = errno;

	assert(arg!=NULL);
	errno = 0;
	*value = strtol(arg, &endptr, 0);
	if (errno != 0 || *endptr!='\0' || endptr==arg) {
		return 1;
	}
	errno = errno_save;
	return 0;
}
#endif

#ifdef L_safe_strtoul
int safe_strtoul(char *arg, unsigned long* value)
{
	char *endptr;
	int errno_save = errno;

	assert(arg!=NULL);
	errno = 0;
	*value = strtoul(arg, &endptr, 0);
	if (errno != 0 || *endptr!='\0' || endptr==arg) {
		return 1;
	}
	errno = errno_save;
	return 0;
}
#endif

#ifdef L_safe_strtoll
int safe_strtoll(char *arg, long long* value)
{
	char *endptr;
	int errno_save = errno;

	assert(arg!=NULL);
	errno = 0;
	*value = strtoll(arg, &endptr, 0);
	if (errno != 0 || *endptr!='\0' || endptr==arg) {
		return 1;
	}
	errno = errno_save;
	return 0;
}
#endif

#ifdef L_safe_strtoull
int safe_strtoull(char *arg, unsigned long long* value)
{
	char *endptr;
	int errno_save = errno;

	assert(arg!=NULL);
	errno = 0;
	*value = strtoull(arg, &endptr, 0);
	if (errno != 0 || *endptr!='\0' || endptr==arg) {
		return 1;
	}
	errno = errno_save;
	return 0;
}
#endif

