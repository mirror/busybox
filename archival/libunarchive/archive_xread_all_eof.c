/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unarchive.h"
#include "libbb.h"

ssize_t archive_xread_all_eof(archive_handle_t *archive_handle, unsigned char *buf, size_t count)
{
	ssize_t size;

	size = full_read(archive_handle->src_fd, buf, count);
	if ((size != 0) && (size != count)) {
		bb_perror_msg_and_die("Short read, read %ld of %ld", (long)size, (long)count);
	}
	return(size);
}
