/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdlib.h>

#include "libbb.h"
#include "unarchive.h"

char get_header_tar_gz(archive_handle_t *archive_handle)
{
	unsigned char magic[2];

	/* Can't lseek over pipes */
	archive_handle->seek = seek_by_read;

	xread(archive_handle->src_fd, &magic, 2);
	if ((magic[0] != 0x1f) || (magic[1] != 0x8b)) {
		bb_error_msg_and_die("invalid gzip magic");
	}

	check_header_gzip(archive_handle->src_fd);

	archive_handle->src_fd = open_transformer(archive_handle->src_fd, inflate_gunzip);
	archive_handle->offset = 0;
	while (get_header_tar(archive_handle) == EXIT_SUCCESS) /**/;

	/* Can only do one file at a time */
	return EXIT_FAILURE;
}
