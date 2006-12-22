/* vi: set sw=4 ts=4: */
/*
 * Copyright 2002 Glenn McGrath
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "unarchive.h"

void data_extract_to_buffer(archive_handle_t *archive_handle)
{
	unsigned int size = archive_handle->file_header->size;

	archive_handle->buffer = xzalloc(size + 1);
	xread(archive_handle->src_fd, archive_handle->buffer, size);
}
