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

#include <unistd.h>

#include "libbb.h"
#include "unarchive.h"

extern void archive_copy_file(const archive_handle_t *archive_handle, const int dst_fd)
{
	char buffer[512];
	off_t chunksize = archive_handle->file_header->size;

	while (chunksize != 0) {
		size_t size;
		if (chunksize > 512) {
			size = 512;
		} else {
			size = chunksize;
		}
//		archive_xread_all(archive_handle, buffer, size);
		size = archive_xread(archive_handle, buffer, size);

		if (write(dst_fd, buffer, size) != size) {
			error_msg_and_die ("Short write");
		}
		chunksize -= size;
	}

	return;
}
