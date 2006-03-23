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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unarchive.h"
#include "libbb.h"

extern ssize_t archive_xread_all_eof(archive_handle_t *archive_handle, unsigned char *buf, size_t count)
{
	ssize_t size;

	size = bb_full_read(archive_handle->src_fd, buf, count);
	if ((size != 0) && (size != count)) {
		bb_perror_msg_and_die("Short read, read %ld of %ld", (long)size, (long)count);
	}
	return(size);
}
