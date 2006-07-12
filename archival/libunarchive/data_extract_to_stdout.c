/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "unarchive.h"
#include <unistd.h>

void data_extract_to_stdout(archive_handle_t *archive_handle)
{
	bb_copyfd_size(archive_handle->src_fd, STDOUT_FILENO, archive_handle->file_header->size);
}
