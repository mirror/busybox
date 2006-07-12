/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unarchive.h"
#include "libbb.h"

void archive_xread_all(const archive_handle_t *archive_handle, void *buf, const size_t count)
{
	ssize_t size;

	size = bb_full_read(archive_handle->src_fd, buf, count);
	if (size != count) {
		bb_error_msg_and_die("Short read");
	}
	return;
}
