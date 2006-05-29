/*
 * Copyright 2002 Glenn McGrath
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "unarchive.h"

void data_extract_to_buffer(archive_handle_t *archive_handle)
{
	const unsigned int size = archive_handle->file_header->size;

	archive_handle->buffer = xzalloc(size + 1);

	archive_xread_all(archive_handle, archive_handle->buffer, size);
}
