/*
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

#include "libbb.h"
#include "unarchive.h"

extern void data_extract_to_buffer(archive_handle_t *archive_handle)
{
	const unsigned int size = archive_handle->file_header->size;

	archive_handle->buffer = xmalloc(size + 1);

	archive_xread_all(archive_handle, archive_handle->buffer, size);
	archive_handle->buffer[size] = '\0';
}
