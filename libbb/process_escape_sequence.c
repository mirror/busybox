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
#include "libbb.h"

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

	n = 0;
	q = *ptr;

	num_digits = 0;
	do {
		if (((unsigned int)(*q - '0')) <= 7) {
			r = n * 8 + (*q - '0');
			if (r <= UCHAR_MAX) {
				n = r;
				++q;
				if (++num_digits < 3) {
					continue;
				}
			}
		}
		break;
	} while (1);

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
