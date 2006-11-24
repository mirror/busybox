/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <assert.h>
#include "libbb.h"

int safe_strtod(const char *arg, double* value)
{
	char *endptr;
	int errno_save = errno;

	assert(arg!=NULL);
	errno = 0;
	*value = strtod(arg, &endptr);
	if (errno != 0 || *endptr != '\0' || endptr == arg) {
		return 1;
	}
	errno = errno_save;
	return 0;
}

int safe_strtoull(const char *arg, unsigned long long* value)
{
	char *endptr;
	int errno_save = errno;

	assert(arg!=NULL);
	if (!isdigit(arg[0])) /* strtouXX takes minus signs w/o error! :( */
		return 1;
	errno = 0;
	*value = strtoull(arg, &endptr, 0);
	if (errno != 0 || *endptr != '\0' || endptr == arg) {
		return 1;
	}
	errno = errno_save;
	return 0;
}

int safe_strtoll(const char *arg, long long* value)
{
	char *endptr;
	int errno_save = errno;

	assert(arg!=NULL);
	errno = 0;
	*value = strtoll(arg, &endptr, 0);
	if (errno != 0 || *endptr != '\0' || endptr == arg) {
		return 1;
	}
	errno = errno_save;
	return 0;
}

int safe_strtoul(const char *arg, unsigned long* value)
{
	char *endptr;
	int errno_save = errno;

	assert(arg!=NULL);
	if (!isdigit(arg[0])) /* strtouXX takes minus signs w/o error! :( */
		return 1;
	errno = 0;
	*value = strtoul(arg, &endptr, 0);
	if (errno != 0 || *endptr != '\0' || endptr == arg) {
		return 1;
	}
	errno = errno_save;
	return 0;
}

int safe_strtol(const char *arg, long* value)
{
	char *endptr;
	int errno_save = errno;

	assert(arg!=NULL);
	errno = 0;
	*value = strtol(arg, &endptr, 0);
	if (errno != 0 || *endptr != '\0' || endptr == arg) {
		return 1;
	}
	errno = errno_save;
	return 0;
}

/* TODO: This is what uclibc is doing. Try to do the same? */

#if 0
#if defined __HAVE_ELF__

# define strong_alias(name, aliasname) _strong_alias(name, aliasname)
# define _strong_alias(name, aliasname) \
  extern __typeof (name) aliasname __attribute__ ((alias (#name)));

#else /* !defined __HAVE_ELF__ */

#  define strong_alias(name, aliasname) _strong_alias (name, aliasname)
#  define _strong_alias(name, aliasname) \
	__asm__(".global " __C_SYMBOL_PREFIX__ #aliasname "\n" \
	        ".set " __C_SYMBOL_PREFIX__ #aliasname "," __C_SYMBOL_PREFIX__ #name);

#endif
#endif

int safe_strtoi(const char *arg, int* value)
{
	int error;
	long lvalue;
	if (sizeof(long) == sizeof(int))
		return safe_strtol(arg, (long*)value);
	lvalue = *value;
	error = safe_strtol(arg, &lvalue);
	if (lvalue < INT_MIN || lvalue > INT_MAX)
		return 1;
	*value = (int) lvalue;
	return error;
}

int safe_strtou(const char *arg, unsigned* value)
{
	int error;
	unsigned long lvalue;
	if (sizeof(unsigned long) == sizeof(unsigned))
		return safe_strtoul(arg, (unsigned long*)value);
	lvalue = *value;
	error = safe_strtoul(arg, &lvalue);
	if (lvalue > UINT_MAX)
		return 1;
	*value = (unsigned) lvalue;
	return error;
}

int BUG_safe_strtou32_unimplemented(void);
int safe_strtou32(const char *arg, uint32_t* value)
{
	if (sizeof(uint32_t) == sizeof(unsigned))
		return safe_strtou(arg, (unsigned*)value);
	if (sizeof(uint32_t) == sizeof(unsigned long))
		return safe_strtoul(arg, (unsigned long*)value);
	return BUG_safe_strtou32_unimplemented();
}
