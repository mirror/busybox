/*
 *  Copyright (C) 2002 by Glenn McGrath
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libbb.h"
#include "unarchive.h"

/*
 *	Reassign the subarchive metadata parser based on the filename extension
 *  e.g. if its a .tar.gz modify archive_handle->sub_archive to process a .tar.gz
 *       or if its a .tar.bz2 make archive_handle->sub_archive handle that
 */
extern char filter_accept_list_reassign(archive_handle_t *archive_handle)
{
	/* Check the file entry is in the accept list */
	if (find_list_entry(archive_handle->accept, archive_handle->file_header->name)) {
		const char *name_ptr;

		/* Extract the last 2 extensions */
		name_ptr = strrchr(archive_handle->file_header->name, '.');

		/* Modify the subarchive handler based on the extension */
#ifdef CONFIG_FEATURE_DEB_TAR_GZ
		if (strcmp(name_ptr, ".gz") == 0) {
			archive_handle->action_data_subarchive = get_header_tar_gz;
			return(EXIT_SUCCESS);
		}
#endif
#ifdef CONFIG_FEATURE_DEB_TAR_BZ2
		if (strcmp(name_ptr, ".bz2") == 0) {
			archive_handle->action_data_subarchive = get_header_tar_bz2;
			return(EXIT_SUCCESS);
		}
#endif
	}
	return(EXIT_FAILURE);
}
