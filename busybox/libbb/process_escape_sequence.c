/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) Manuel Nova III <mnovoa3@bellsouth.net>
 * and Vladimir Oleynik <vodz@usa.net> 
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



char process_escape_sequence(const char **ptr)
{
	static const char charmap[] = {
		'a',  'b',  'f',  'n',  'r',  't',  'v',  '\\', 0,
		'\a', '\b', '\f', '\n', '\r', '\t', '\v', '\\', '\\' };

	const char *p;
	const char *q;
	int num_digits;
	unsigned int n;
	
	n = 0;
	q = *ptr;

	for ( num_digits = 0 ; num_digits < 3 ; ++num_digits) {
		if ((*q < '0') || (*q > '7')) { /* not a digit? */
			break;
		}
		n = n * 8 + (*q++ - '0');
	}

	if (num_digits == 0) {	/* mnemonic escape sequence? */
		for (p=charmap ; *p ; p++) {
			if (*p == *q) {
				q++;
				break;
			}
		}
		n = *(p+(sizeof(charmap)/2));
	}

	   /* doesn't hurt to fall through to here from mnemonic case */
	if (n > UCHAR_MAX) {	/* is octal code too big for a char? */
		n /= 8;			/* adjust value and */
		--q;				/* back up one char */
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
