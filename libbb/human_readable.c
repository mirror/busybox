/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) tons of folks.  Tracking down who wrote what
 * isn't something I'm going to worry about...  If you wrote something
 * here, please feel free to acknowledge your work.
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
 * Based in part on code from sash, Copyright (c) 1999 by David I. Bell 
 * Permission has been granted to redistribute this code under the GPL.
 *
 */

#include <stdio.h>
#include "libbb.h"

static char buffer[10];
static const char *suffixes[] = { "", "k", "M", "G", "T" };

const char *make_human_readable_str(unsigned long val, unsigned long not_hr)
{
	int suffix, base;

	if (not_hr)
		sprintf(buffer, "%lu", val);
	else
		for (suffix = 0, base = 1; suffix < 5; suffix++, base <<= 10) {
			if (val < (base << 10)) {
				if (suffix && val < 10 * base)
					sprintf(buffer, "%lu.%lu%s", val / base,
							(val % base) * 10 / base, suffixes[suffix]);
				else
					sprintf(buffer, "%lu%s", val / base, suffixes[suffix]);
				break;
			}
		}

	return buffer;
}
