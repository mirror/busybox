/*
 *  Copyright (C) by Glenn McGrath <bug1@optushome.com.au>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <string.h>

#include "libbb.h"
#include "unarchive.h"

extern void data_extract_all_prefix(archive_handle_t *archive_handle)
{
	char *name_ptr = archive_handle->file_header->name;

	name_ptr += strspn(name_ptr, "./");
	if (name_ptr[0] != '\0') {
		archive_handle->file_header->name = xmalloc(strlen(archive_handle->buffer) + 2 + strlen(name_ptr));
		strcpy(archive_handle->file_header->name, archive_handle->buffer);
		strcat(archive_handle->file_header->name, name_ptr);
		data_extract_all(archive_handle);
	}
	return;
}
