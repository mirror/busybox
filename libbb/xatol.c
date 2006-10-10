/* vi: set sw=4 ts=4: */
/*
 * ascii-to-numbers implementations for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

unsigned long long xstrtoull(const char *numstr, int base)
{
	unsigned long long r;
	int old_errno;
	char *e;
	if ((*numstr == '-') || (isspace)(*numstr))
		bb_error_msg_and_die("invalid number '%s'", numstr);
	old_errno = errno;
	errno = 0;
	r = strtoull(numstr, &e, base);
	if (errno || (numstr == e) || *e)
		/* Error / no digits / illegal trailing chars */
		bb_error_msg_and_die("invalid number '%s'", numstr);
	/* No error. So restore errno. */
	errno = old_errno;
	return r;
}

unsigned long long xatoull(const char *numstr)
{
	return xstrtoull(numstr, 10);
}

unsigned long xstrtoul_range_sfx(const char *numstr, int base,
		unsigned long lower,
		unsigned long upper,
		const struct suffix_mult *suffixes)
{
	unsigned long r;
	int old_errno;
	char *e;

	/* Disallow '-' and any leading whitespace.  Speed isn't critical here
	 * since we're parsing commandline args.  So make sure we get the
	 * actual isspace function rather than a lnumstrer macro implementaion. */
	if ((*numstr == '-') || (isspace)(*numstr))
		goto inval;

	/* Since this is a lib function, we're not allowed to reset errno to 0.
	 * Doing so could break an app that is deferring checking of errno.
	 * So, save the old value so that we can restore it if successful. */
	old_errno = errno;
	errno = 0;
	r = strtoul(numstr, &e, base);
	/* Do the initial validity check.  Note: The standards do not
	 * guarantee that errno is set if no digits were found.  So we
	 * must test for this explicitly. */
	if (errno || (numstr == e))
		goto inval; /* error / no digits / illegal trailing chars */

	errno = old_errno;	/* Ok.  So restore errno. */

	/* Do optional suffix parsing.  Allow 'empty' suffix tables.
	 * Note that we also allow nul suffixes with associated multipliers,
	 * to allow for scaling of the numstr by some default multiplier. */
	if (suffixes) {
		while (suffixes->suffix) {
			if (strcmp(suffixes->suffix, e) == 0) {
				if (ULONG_MAX / suffixes->mult < r)
					goto range; /* overflow! */
				++e;
				r *= suffixes->mult;
				break;
			}
			++suffixes;
		}
	}

	/* Note: trailing space is an error.
	   It would be easy enough to allow though if desired. */
	if (*e)
		goto inval;
	/* Finally, check for range limits. */
	if (r >= lower && r <= upper)
		return r;
 range:
	bb_error_msg_and_die("number %s is not in %lu..%lu range",
		numstr, lower, upper);
 inval:
	bb_error_msg_and_die("invalid number '%s'", numstr);
}

unsigned long xstrtoul_range(const char *numstr, int base,
		unsigned long lower,
		unsigned long upper)
{
	return xstrtoul_range_sfx(numstr, base, lower, upper, NULL);
}

unsigned long xstrtoul(const char *numstr, int base)
{
	return xstrtoul_range_sfx(numstr, base, 0, ULONG_MAX, NULL);
}

unsigned long xatoul_range_sfx(const char *numstr,
		unsigned long lower,
		unsigned long upper,
		const struct suffix_mult *suffixes)
{
	return xstrtoul_range_sfx(numstr, 10, lower, upper, suffixes);
}

unsigned long xatoul_sfx(const char *numstr,
		const struct suffix_mult *suffixes)
{
	return xstrtoul_range_sfx(numstr, 10, 0, ULONG_MAX, suffixes);
}

unsigned long xatoul_range(const char *numstr,
		unsigned long lower,
		unsigned long upper)
{
	return xstrtol_range_sfx(numstr, 10, lower, upper, NULL);
}

unsigned long xatoul(const char *numstr)
{
	return xatoul_sfx(numstr, NULL);
}

/* Signed ones */

long xstrtol_range_sfx(const char *numstr, int base,
		long lower,
		long upper,
		const struct suffix_mult *suffixes)
{
	unsigned long u = LONG_MAX;
	long r;
	const char *p = numstr;

	if ((p[0] == '-') && (p[1] != '+')) {
		++p;
		++u;	/* two's complement */
	}

	r = xstrtoul_range_sfx(p, base, 0, u, suffixes);

	if (*numstr == '-') {
		r = -r;
	}

	if (r < lower || r > upper) {
		bb_error_msg_and_die("number %s is not in %ld..%ld range",
				numstr, lower, upper);
	}

	return r;
}

long xstrtol_range(const char *numstr, int base, long lower, long upper)
{
	return xstrtol_range_sfx(numstr, base, lower, upper, NULL);
}

long xatol_range_sfx(const char *numstr,
		long lower,
		long upper,
		const struct suffix_mult *suffixes)
{
	return xstrtol_range_sfx(numstr, 10, lower, upper, suffixes);
}

long xatol_range(const char *numstr, long lower, long upper)
{
	return xstrtol_range_sfx(numstr, 10, lower, upper, NULL);
}

long xatol_sfx(const char *numstr, const struct suffix_mult *suffixes)
{
	return xstrtol_range_sfx(numstr, 10, LONG_MIN, LONG_MAX, suffixes);
}

long xatol(const char *numstr)
{
	return xstrtol_range_sfx(numstr, 10, LONG_MIN, LONG_MAX, NULL);
}

/* Others */

unsigned xatou(const char *numstr)
{
	return xatoul_range(numstr, 0, UINT_MAX);
}

int xatoi_range(const char *numstr, int lower, int upper)
{
	return xatol_range(numstr, lower, upper);
}

int xatoi(const char *numstr)
{
	return xatol_range(numstr, INT_MIN, INT_MAX);
}

int xatoi_u(const char *numstr)
{
	return xatoul_range(numstr, 0, INT_MAX);
}

uint32_t xatou32(const char *numstr)
{
	return xatoul_range(numstr, 0, 0xffffffff);
}

uint16_t xatou16(const char *numstr)
{
	return xatoul_range(numstr, 0, 0xffff);
}
