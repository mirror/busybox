/* vi: set sw=4 ts=4: */
/*
 * June 30, 2001                 Manuel Novoa III
 *
 * All-integer version (hey, not everyone has floating point) of
 * make_human_readable_str, modified from similar code I had written
 * for busybox several months ago.
 *
 * Notes:
 *   1) I'm using an unsigned long long to hold the product size * block_size,
 *      as df (which calls this routine) could request a representation of a
 *      partition size in bytes > max of unsigned long.  If long longs aren't
 *      available, it would be possible to do what's needed using polynomial
 *      representations (say, powers of 1024) and manipulating coefficients.
 *      The base ten "bytes" output could be handled similarly.
 *
 *   2) This routine always outputs a decimal point and a tenths digit when
 *      display_unit != 0.  Hence, it isn't uncommon for the returned string
 *      to have a length of 5 or 6.
 *
 *      It might be nice to add a flag to indicate no decimal digits in
 *      that case.  This could be either an additional parameter, or a
 *      special value of display_unit.  Such a flag would also be nice for du.
 *
 *      Some code to omit the decimal point and tenths digit is sketched out
 *      and "#if 0"'d below.
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

const char* FAST_FUNC make_human_readable_str(unsigned long long size,
	unsigned long block_size, unsigned long display_unit)
{
	/* The code will adjust for additional (appended) units */
	static const char unit_chars[] ALIGN1 = {
		'\0', 'K', 'M', 'G', 'T', 'P', 'E'
	};
	static const char fmt[] ALIGN1 = "%llu";
	static const char fmt_tenths[] ALIGN1 = "%llu.%d%c";

	static char str[21] ALIGN1;  /* Sufficient for 64 bit unsigned integers */

	unsigned long long val;
	int frac;
	const char *u;
	const char *f;
	smallint no_tenths;

	if (size == 0)
		return "0";

	/* If block_size is 0 then do not print tenths */
	no_tenths = 0;
	if (block_size == 0) {
		no_tenths = 1;
		block_size = 1;
	}

	u = unit_chars;
	val = size * block_size;
	f = fmt;
	frac = 0;

	if (display_unit) {
		val += display_unit/2;	/* Deal with rounding */
		val /= display_unit;	/* Don't combine with the line above!!! */
		/* will just print it as ulonglong (below) */
	} else {
		while ((val >= 1024)
		 && (u < unit_chars + sizeof(unit_chars) - 1)
		) {
			f = fmt_tenths;
			u++;
			frac = (((int)(val % 1024)) * 10 + 1024/2) / 1024;
			val /= 1024;
		}
		if (frac >= 10) {		/* We need to round up here. */
			++val;
			frac = 0;
		}
#if 1
		/* Sample code to omit decimal point and tenths digit. */
		if (no_tenths) {
			if (frac >= 5) {
				++val;
			}
			f = "%llu%*c" /* fmt_no_tenths */;
			frac = 1;
		}
#endif
	}

	/* If f==fmt then 'frac' and 'u' are ignored. */
	snprintf(str, sizeof(str), f, val, frac, *u);

	return str;
}
