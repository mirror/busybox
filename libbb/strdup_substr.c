/* vi: set sw=4 ts=4: */
/*
 * Mini strdup_substr function.
 *
 * Copyright (C) 2001  Mark Whitley.
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
 */

/* Return a substring of STR, starting at index START and ending at END,
 * allocated on the heap.  */

#include "libbb.h"

char *strdup_substr(const char *str, int start, int end)
{
	int size = end - start + 1;
	char *newstr = xmalloc(size);
	memcpy(newstr, str+start, size-1);
	newstr[size-1] = '\0';
	return newstr;
}
