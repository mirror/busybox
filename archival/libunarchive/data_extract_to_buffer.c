#include <stdlib.h>
#include <stdio.h>
#include "unarchive.h"
#include "libbb.h"

extern void data_extract_to_buffer(archive_handle_t *archive_handle)
{
	archive_handle->buffer = xmalloc(archive_handle->file_header->size + 1);
	
	xread_all(archive_handle->src_fd, archive_handle->buffer, archive_handle->file_header->size);
}
