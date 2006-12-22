/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdlib.h>

#include "unarchive.h"
#include "libbb.h"

/*  If we are reading through a pipe(), or from stdin then we can't lseek,
 *  we must read and discard the data to skip over it.
 */
void seek_by_read(const archive_handle_t *archive_handle, const unsigned int jump_size)
{
	if (jump_size)
		bb_copyfd_exact_size(archive_handle->src_fd, -1, jump_size);
}
