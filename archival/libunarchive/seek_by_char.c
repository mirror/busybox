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

#include "unarchive.h"
#include "busybox.h"

/*	If we are reading through a pipe(), or from stdin then we cant lseek,
 *  we must read and discard the data to skip over it.
 *
 *  TODO: rename to seek_by_read
 */
extern void seek_by_char(const archive_handle_t *archive_handle, const unsigned int jump_size)
{
	if (jump_size) {
		bb_full_fd_action(archive_handle->src_fd, -1, jump_size, NULL);
	}
}
