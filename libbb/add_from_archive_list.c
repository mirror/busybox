/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "libbb.h"

file_headers_t *add_from_archive_list(file_headers_t *master_list, file_headers_t *new_list, const char *filename)
{
	file_headers_t *list_ptr;

	list_ptr = master_list;
	while (list_ptr != NULL) {
		if (strcmp(filename, list_ptr->name) == 0) {
			return(append_archive_list(new_list, list_ptr));
		}
		list_ptr = list_ptr->next;
	}
	return(NULL);
}