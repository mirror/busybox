/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2002 by Glenn McGrath
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "unarchive.h"

/*
 * Accept names that are in the accept list and not in the reject list
 */
char filter_accept_reject_list(archive_handle_t *archive_handle)
{
	const char *key = archive_handle->file_header->name;
	const llist_t *reject_entry = find_list_entry2(archive_handle->reject, key);
	const llist_t *accept_entry;

	/* If the key is in a reject list fail */
	if (reject_entry) {
		return EXIT_FAILURE;
	}
	accept_entry = find_list_entry2(archive_handle->accept, key);

	/* Fail if an accept list was specified and the key wasnt in there */
	if ((accept_entry == NULL) && archive_handle->accept) {
		return EXIT_FAILURE;
	}

	/* Accepted */
	return EXIT_SUCCESS;
}
