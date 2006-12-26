/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

/* On exit: errno = 0 only if there was non-empty, '\0' terminated value
 * errno = EINVAL if value was not '\0' terminated, but othervise ok
 *    Return value is still valid, caller should just check whether end[0]
 *    is a valid terminating char for particular case. OTOH, if caller
 *    requires '\0' terminated input, [s]he can just check errno == 0.
 * errno = ERANGE if value had alphanumeric terminating char ("1234abcg").
 * errno = ERANGE if value is out of range, missing, etc.
 * errno = ERANGE if value had minus sign for strtouXX (even "-0" is not ok )
 *    return value is all-ones in this case.
 */

static unsigned long long ret_ERANGE(void)
{
	errno = ERANGE; /* this ain't as small as it looks (on glibc) */
	return ULLONG_MAX;
}

static unsigned long long handle_errors(unsigned long long v, char **endp, char *endptr)
{
	if (endp) *endp = endptr;

	/* Check for the weird "feature":
	 * a "-" string is apparently a valid "number" for strto[u]l[l]!
	 * It returns zero and errno is 0! :( */
	if (endptr[-1] == '-')
		return ret_ERANGE();

	/* errno is already set to ERANGE by strtoXXX if value overflowed */
	if (endptr[0]) {
		/* "1234abcg" or out-of-range? */
		if (isalnum(endptr[0]) || errno)
			return ret_ERANGE();
		/* good number, just suspicious terminator */
		errno = EINVAL;
	}
	return v;
}


unsigned long long bb_strtoull(const char *arg, char **endp, int base)
{
	unsigned long long v;
	char *endptr;

	/* strtoul("  -4200000000") returns 94967296, errno 0 (!) */
	/* I don't think that this is right. Preventing this... */
	if (!isalnum(arg[0])) return ret_ERANGE();

	/* not 100% correct for lib func, but convenient for the caller */
	errno = 0;
	v = strtoull(arg, &endptr, base);
	return handle_errors(v, endp, endptr);
}

long long bb_strtoll(const char *arg, char **endp, int base)
{
	unsigned long long v;
	char *endptr;

	if (arg[0] != '-' && !isalnum(arg[0])) return ret_ERANGE();
	errno = 0;
	v = strtoll(arg, &endptr, base);
	return handle_errors(v, endp, endptr);
}

#if ULONG_MAX != ULLONG_MAX
unsigned long bb_strtoul(const char *arg, char **endp, int base)
{
	unsigned long v;
	char *endptr;

	if (!isalnum(arg[0])) return ret_ERANGE();
	errno = 0;
	v = strtoul(arg, &endptr, base);
	return handle_errors(v, endp, endptr);
}

long bb_strtol(const char *arg, char **endp, int base)
{
	long v;
	char *endptr;

	if (arg[0] != '-' && !isalnum(arg[0])) return ret_ERANGE();
	errno = 0;
	v = strtol(arg, &endptr, base);
	return handle_errors(v, endp, endptr);
}
#endif

#if UINT_MAX != ULONG_MAX
unsigned bb_strtou(const char *arg, char **endp, int base)
{
	unsigned long v;
	char *endptr;

	if (!isalnum(arg[0])) return ret_ERANGE();
	errno = 0;
	v = strtoul(arg, &endptr, base);
	if (v > UINT_MAX) return ret_ERANGE();
	return handle_errors(v, endp, endptr);
}

int bb_strtoi(const char *arg, char **endp, int base)
{
	long v;
	char *endptr;

	if (arg[0] != '-' && !isalnum(arg[0])) return ret_ERANGE();
	errno = 0;
	v = strtol(arg, &endptr, base);
	if (v > INT_MAX) return ret_ERANGE();
	if (v < INT_MIN) return ret_ERANGE();
	return handle_errors(v, endp, endptr);
}
#endif

/* Floating point */

#if 0

#include <math.h>  /* just for HUGE_VAL */
#define NOT_DIGIT(a) (((unsigned char)(a-'0')) > 9)
double bb_strtod(const char *arg, char **endp)
{
	double v;
	char *endptr;

	if (arg[0] != '-' && NOT_DIGIT(arg[0])) goto err;
	errno = 0;
	v = strtod(arg, &endptr);
	if (endp) *endp = endptr;
	if (endptr[0]) {
		/* "1234abcg" or out-of-range? */
		if (isalnum(endptr[0]) || errno) {
 err:
			errno = ERANGE;
			return HUGE_VAL;
		}
		/* good number, just suspicious terminator */
		errno = EINVAL;
	}
	return v;
}

#endif
