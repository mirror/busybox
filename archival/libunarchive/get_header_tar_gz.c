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

#include <stdlib.h>

#include "libbb.h"
#include "unarchive.h"

extern char get_header_tar_gz(archive_handle_t *archive_handle)
{
	unsigned char magic[2];

	/* Cant lseek over pipe's */
	archive_handle->seek = seek_by_char;

	archive_xread_all(archive_handle, &magic, 2);
	if ((magic[0] != 0x1f) || (magic[1] != 0x8b)) {
		bb_error_msg_and_die("Invalid gzip magic");
	}

	check_header_gzip(archive_handle->src_fd);

	archive_handle->src_fd = open_transformer(archive_handle->src_fd, inflate_gunzip);
	archive_handle->offset = 0;
	while (get_header_tar(archive_handle) == EXIT_SUCCESS);

	/* Can only do one file at a time */
	return(EXIT_FAILURE);
}
