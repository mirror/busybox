/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) (C) 2003  Vladimir Oleynik  <dzo@simtreas.ru>
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

/*
   This function make special for recursive actions with usage
   concat_path_file(path, filename)
   and skiping "." and ".." directory entries
*/

#include "libbb.h"

extern char *concat_subpath_file(const char *path, const char *f)
{
	if(f && *f == '.' && (!f[1] || (f[1] == '.' && !f[2])))
		return NULL;
	return concat_path_file(path, f);
}
