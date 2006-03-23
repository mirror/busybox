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

#include <fnmatch.h>
#include <stdlib.h>

#include "unarchive.h"

/*
 * Accept names that are in the accept list and not in the reject list
 */
extern char filter_accept_reject_list(archive_handle_t *archive_handle)
{
	const char *key = archive_handle->file_header->name;
	const llist_t *accept_entry = find_list_entry(archive_handle->accept, key);
	const llist_t *reject_entry = find_list_entry(archive_handle->reject, key);

	/* If the key is in a reject list fail */
	if (reject_entry) {
		return(EXIT_FAILURE);
	}

	/* Fail if an accept list was specified and the key wasnt in there */
	if (archive_handle->accept && (accept_entry == NULL)) {
		return(EXIT_FAILURE);
	}

	/* Accepted */
	return(EXIT_SUCCESS);
}
