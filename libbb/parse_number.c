/* vi: set sw=4 ts=4: */
/*
 * bb_xparse_number implementation for busybox
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
#include <errno.h>
#include <assert.h>
#include "libbb.h"

extern
unsigned long bb_xparse_number(const char *numstr,
							   const struct suffix_mult *suffixes)
{
	unsigned long int r;
	char *e;
	int old_errno;
	
	/* Since this is a lib function, we're not allowed to reset errno to 0.
	 * Doing so could break an app that is deferring checking of errno.
	 * So, save the old value so that we can restore it if successful. */
	old_errno = errno;
	errno = 0;
	r = strtoul(numstr, &e, 10);

	if ((numstr != e) && !errno) {
		errno = old_errno;	/* Ok.  So restore errno. */
		if (!*e) {
			return r;
		}
		if (suffixes) {
			assert(suffixes->suffix);	/* No nul suffixes. */
			do {
				if (strcmp(suffixes->suffix, e) == 0) {
					if (ULONG_MAX / suffixes->mult < r) {	/* Overflow! */
						break;
					}
					return r * suffixes->mult;
				}
				++suffixes;
			} while (suffixes->suffix);
		}
	}
	bb_error_msg_and_die("invalid number `%s'", numstr);
}
