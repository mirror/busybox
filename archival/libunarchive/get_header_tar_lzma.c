/* vi: set sw=4 ts=4: */
/*
 * Small lzma deflate implementation.
 * Copyright (C) 2006  Aurelien Jacobs <aurel@gnuage.org>
 *
 * Licensed under GPL v2, see file LICENSE in this tarball for details.
 */

#include "unarchive.h"

char get_header_tar_lzma(archive_handle_t * archive_handle)
{
	/* Can't lseek over pipes */
	archive_handle->seek = seek_by_read;

	archive_handle->src_fd = open_transformer(archive_handle->src_fd, unlzma);
	archive_handle->offset = 0;
	while (get_header_tar(archive_handle) == EXIT_SUCCESS) /**/;

	/* Can only do one file at a time */
	return EXIT_FAILURE;
}
