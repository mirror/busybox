/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) many different people.  
 * If you wrote this, please acknowledge your work.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/* concatenate path and file name to new allocation buffer,
 * not addition '/' if path name already have '/'
*/

#include <string.h>
#include "libbb.h"

extern char *concat_path_file(const char *path, const char *filename)
{
	char *outbuf;
	char *lc;

	if (!path)
	    path="";
	lc = last_char_is(path, '/');
	while (*filename == '/')
		filename++;
	bb_xasprintf(&outbuf, "%s%s%s", path, (lc==NULL)? "/" : "", filename);

	return outbuf;
}
