/* vi: set sw=4 ts=4: */
/*
 * bb_ask_confirmation implementation for busybox
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

/* Read a line from stdin.  If the first non-whitespace char is 'y' or 'Y',
 * return 1.  Otherwise return 0.
 */

#include <stdio.h>
#include <ctype.h>
#include "libbb.h"

int bb_ask_confirmation(void)
{
	int retval = 0;
	int first = 1;
	int c;

	while (((c = getchar()) != EOF) && (c != '\n')) {
		/* Make sure we get the actual function call for isspace,
		 * as speed is not critical here. */
		if (first && !(isspace)(c)) {
			--first;
			if ((c == 'y') || (c == 'Y')) {
				++retval;
			}
		}
	}

	return retval;
}
