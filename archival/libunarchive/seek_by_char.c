/* vi:set ts=4:*/
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdlib.h>

#include "unarchive.h"
#include "libbb.h"



/*	If we are reading through a pipe(), or from stdin then we cant lseek,
 *  we must read and discard the data to skip over it.
 *
 *  TODO: rename to seek_by_read
 */
void seek_by_char(const archive_handle_t *archive_handle, const unsigned int jump_size)
{
	if (jump_size) {
		bb_copyfd_size(archive_handle->src_fd, -1, jump_size);
	}
}
