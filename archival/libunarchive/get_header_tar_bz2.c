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

#include <sys/types.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "libbb.h"
#include "unarchive.h"

extern char get_header_tar_bz2(archive_handle_t *archive_handle)
{
	BZ2_bzReadOpen(archive_handle->src_fd, NULL, 0);

	archive_handle->read = read_bz2;
	archive_handle->seek = seek_by_char;

	archive_handle->offset = 0;
	while (get_header_tar(archive_handle) == EXIT_SUCCESS);

	/* Cleanup */
	BZ2_bzReadClose();
	
	/* Can only do one tar.bz2 per archive */
	return(EXIT_FAILURE);
}

