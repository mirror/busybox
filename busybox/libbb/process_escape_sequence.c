/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) Manuel Novoa III <mjn3@codepoet.org>
 * and Vladimir Oleynik <dzo@simtreas.ru>
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
 *
 */

#include <stdio.h>
#include <limits.h>
#include <ctype.h>
#include "libbb.h"

#define WANT_HEX_ESCAPES 1

/* Usual "this only works for ascii compatible encodings" disclaimer. */
#undef _tolower
#define _tolower(X) ((X)|((char) 0x20))

char bb_process_escape_sequence(const char **ptr)
{
	static const char charmap[] = {
		'a',  'b',  'f',  'n',  'r',  't',  'v',  '\\', 0,
		'\a', '\b', '\f', '\n', '\r', '\t', '\v', '\\', '\\' };

	const char *p;
	const char *q;
	unsigned int num_digits;
	unsigned int r;
	unsigned int n;
	unsigned int d;
	unsigned int base;

	num_digits = n = 0;
	base = 8;
	q = *ptr;

#ifdef WANT_HEX_ESCAPES
	if (*q == 'x') {
		++q;
		base = 16;
		++num_digits;
	}
#endif

	do {
		d = (unsigned int)(*q - '0');
#ifdef WANT_HEX_ESCAPES
		if (d >= 10) {
			d = ((unsigned int)(_tolower(*q) - 'a')) + 10;
		}
#endif

		if (d >= base) {
#ifdef WANT_HEX_ESCAPES
			if ((base == 16) && (!--num_digits)) {
/* 				return '\\'; */
				--q;
			}
#endif
			break;
		}

		r = n * base + d;
		if (r > UCHAR_MAX) {
			break;
		}

		n = r;
		++q;
	} while (++num_digits < 3);

	if (num_digits == 0) {	/* mnemonic escape sequence? */
		p = charmap;
		do {
			if (*p == *q) {
				q++;
				break;
			}
		} while (*++p);
		n = *(p+(sizeof(charmap)/2));
	}

	*ptr = q;

	return (char) n;
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
