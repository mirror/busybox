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

#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <ctype.h>
#include "libbb.h"

#define isodigit(c) ((c) >= '0' && (c) <= '7')
#define hextobin(c) ((c)>='a'&&(c)<='f' ? (c)-'a'+10 : (c)>='A'&&(c)<='F' ? (c)-'A'+10 : (c)-'0')
#define octtobin(c) ((c) - '0')
char bb_process_escape_sequence(const char **ptr)
{
	const char *p, *q;
	unsigned int num_digits, r, n, hexescape;
	static const char charmap[] = {
		'a',  'b',  'f',  'n',  'r',  't',  'v',  '\\', 0,
		'\a', '\b', '\f', '\n', '\r', '\t', '\v', '\\', '\\' };

	n = r = hexescape = num_digits = 0;
	q = *ptr;

	if (*q == 'x') {
		hexescape++;
		++q;
	}

	do {
		if (hexescape && isxdigit(*q)) {
			r = n * 16 + hextobin(*q);
		} else if (isodigit(*q)) {
			r = n * 8 + octtobin(*q);
		}
		if (r <= UCHAR_MAX) {
			n = r;
			++q;
			if (++num_digits < 3) {
				continue;
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
