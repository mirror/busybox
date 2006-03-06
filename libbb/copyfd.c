/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2005 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "busybox.h"
#include "libbb.h"


#if BUFSIZ < 4096
#undef BUFSIZ
#define BUFSIZ 4096
#endif


static ssize_t bb_full_fd_action(int src_fd, int dst_fd, size_t size)
{
	int status = -1;
	size_t total = 0;
	RESERVE_CONFIG_BUFFER(buffer,BUFSIZ);

	if (src_fd < 0) goto out;
	while (!size || total < size)
	{
		ssize_t wrote, xread;

		xread = safe_read(src_fd, buffer,
				(!size || size - total > BUFSIZ) ? BUFSIZ : size - total);

		if (xread > 0) {
			/* A -1 dst_fd means we need to fake it... */
			wrote = (dst_fd < 0) ? xread : bb_full_write(dst_fd, buffer, xread);
			if (wrote < xread) {
				bb_perror_msg(bb_msg_write_error);
				break;
			}
			total += wrote;
			if (total == size) status = 0;
		} else if (xread < 0) {
			bb_perror_msg(bb_msg_read_error);
			break;
		} else if (xread == 0) {
			/* All done. */
			status = 0;
			break;
		}
	}

out:
	RELEASE_CONFIG_BUFFER(buffer);

	return status ? status : (ssize_t)total;
}


int bb_copyfd_size(int fd1, int fd2, const off_t size)
{
	if (size) {
		return(bb_full_fd_action(fd1, fd2, size));
	}
	return(0);
}

int bb_copyfd_eof(int fd1, int fd2)
{
	return(bb_full_fd_action(fd1, fd2, 0));
}
