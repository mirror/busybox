#include <stdlib.h>
#include <stdio.h>
#include "unarchive.h"

extern void data_extract_to_stdout(archive_handle_t *archive_handle)
{
	copy_file_chunk_fd(archive_handle->src_fd, fileno(stdout), archive_handle->file_header->size);
}
