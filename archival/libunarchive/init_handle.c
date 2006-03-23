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
#include <string.h>
#include "libbb.h"
#include "unarchive.h"

archive_handle_t *init_handle(void)
{
	archive_handle_t *archive_handle;

	/* Initialise default values */
	archive_handle = xmalloc(sizeof(archive_handle_t));
	memset(archive_handle, 0, sizeof(archive_handle_t));
	archive_handle->file_header = xmalloc(sizeof(file_header_t));
	archive_handle->action_header = header_skip;
	archive_handle->action_data = data_skip;
	archive_handle->filter = filter_accept_all;
	archive_handle->seek = seek_by_jump;

	return(archive_handle);
}
