/* vi: set sw=4 ts=4: */
/*
 * xgetularg* implementations for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
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
 *
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>
#include "libbb.h"

#ifdef L_xgetularg_bnd_sfx
extern
unsigned long bb_xgetularg_bnd_sfx(const char *arg, int base,
								   unsigned long lower,
								   unsigned long upper,
								   const struct suffix_mult *suffixes)
{
	unsigned long r;
	int old_errno;
	char *e;

	assert(arg);

	/* Disallow '-' and any leading whitespace.  Speed isn't critical here
	 * since we're parsing commandline args.  So make sure we get the
	 * actual isspace function rather than a larger macro implementaion. */
	if ((*arg == '-') || (isspace)(*arg)) {
		bb_show_usage();
	}

	/* Since this is a lib function, we're not allowed to reset errno to 0.
	 * Doing so could break an app that is deferring checking of errno.
	 * So, save the old value so that we can restore it if successful. */
	old_errno = errno;
	errno = 0;
	r = strtoul(arg, &e, base);
	/* Do the initial validity check.  Note: The standards do not
	 * guarantee that errno is set if no digits were found.  So we
	 * must test for this explicitly. */
	if (errno || (arg == e)) {	/* error or no digits */
		bb_show_usage();
	}
	errno = old_errno;	/* Ok.  So restore errno. */

	/* Do optional suffix parsing.  Allow 'empty' suffix tables.
	 * Note that we also all nul suffixes with associated multipliers,
	 * to allow for scaling of the arg by some default multiplier. */

	if (suffixes) {
		while (suffixes->suffix) {
			if (strcmp(suffixes->suffix, e) == 0) {
				if (ULONG_MAX / suffixes->mult < r) {	/* Overflow! */
					bb_show_usage();
				}
				++e;
				r *= suffixes->mult;
				break;
 			}
			++suffixes;
		}
	}

	/* Finally, check for illegal trailing chars and range limits. */
	/* Note: although we allow leading space (via stroul), trailing space
	 * is an error.  It would be easy enough to allow though if desired. */
	if (*e || (r < lower) || (r > upper)) {
		bb_show_usage();
	}

	return r;
}
#endif

#ifdef L_xgetlarg_bnd_sfx
extern
long bb_xgetlarg_bnd_sfx(const char *arg, int base,
						 long lower,
						 long upper,
						 const struct suffix_mult *suffixes)
{
	unsigned long u = LONG_MAX;
	long r;
	const char *p = arg;

	if ((*p == '-') && (p[1] != '+')) {
		++p;
#if LONG_MAX == (-(LONG_MIN + 1))
		++u;	/* two's complement */
#endif
	}

	r = bb_xgetularg_bnd_sfx(p, base, 0, u, suffixes);

	if (*arg == '-') {
		r = -r;
	}

	if ((r < lower) || (r > upper)) {
		bb_show_usage();
	}

	return r;
}
#endif

#ifdef L_getlarg10_sfx
extern
long bb_xgetlarg10_sfx(const char *arg, const struct suffix_mult *suffixes)
{
	return bb_xgetlarg_bnd_sfx(arg, 10, LONG_MIN, LONG_MAX, suffixes);
}
#endif

#ifdef L_xgetularg_bnd
extern
unsigned long bb_xgetularg_bnd(const char *arg, int base,
							   unsigned long lower,
							   unsigned long upper)
{
	return bb_xgetularg_bnd_sfx(arg, base, lower, upper, NULL);
}
#endif

#ifdef L_xgetularg10_bnd
extern
unsigned long bb_xgetularg10_bnd(const char *arg,
								 unsigned long lower,
								 unsigned long upper)
{
	return bb_xgetularg_bnd(arg, 10, lower, upper);
}
#endif

#ifdef L_xgetularg10
extern
unsigned long bb_xgetularg10(const char *arg)
{
	return bb_xgetularg10_bnd(arg, 0, ULONG_MAX);
}
#endif
