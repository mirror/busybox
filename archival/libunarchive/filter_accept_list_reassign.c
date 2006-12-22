/* vi: set sw=4 ts=4: */
/*
 *  Copyright (C) 2002 by Glenn McGrath
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "unarchive.h"

/*
 * Reassign the subarchive metadata parser based on the filename extension
 * e.g. if its a .tar.gz modify archive_handle->sub_archive to process a .tar.gz
 * or if its a .tar.bz2 make archive_handle->sub_archive handle that
 */
char filter_accept_list_reassign(archive_handle_t *archive_handle)
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
			return EXIT_SUCCESS;
		}
#endif
#ifdef CONFIG_FEATURE_DEB_TAR_BZ2
		if (strcmp(name_ptr, ".bz2") == 0) {
			archive_handle->action_data_subarchive = get_header_tar_bz2;
			return EXIT_SUCCESS;
		}
#endif
		if (ENABLE_FEATURE_DEB_TAR_LZMA && !strcmp(name_ptr, ".lzma")) {
			archive_handle->action_data_subarchive = get_header_tar_lzma;
			return EXIT_SUCCESS;
		}
	}
	return EXIT_FAILURE;
}
