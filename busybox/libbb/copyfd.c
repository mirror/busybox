/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2005 by Erik Andersen <andersen@codepoet.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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


static size_t bb_full_fd_action(int src_fd, int dst_fd, const size_t size2)
{
	int status;
	size_t xread, wrote, total, size = size2;

	if (src_fd < 0) {
		return -1;
	}

	if (size == 0) {
		/* If size is 0 copy until EOF */
		size = ULONG_MAX;
	}

	{
		RESERVE_CONFIG_BUFFER(buffer,BUFSIZ);
		total = 0;
		wrote = 0;
		status = -1;
		while (total < size)
		{
			xread = BUFSIZ;
			if (size < (total + BUFSIZ))
				xread = size - total;
			xread = bb_full_read(src_fd, buffer, xread);
			if (xread > 0) {
				if (dst_fd < 0) {
					/* A -1 dst_fd means we need to fake it... */
					wrote = xread;
				} else {
					wrote = bb_full_write(dst_fd, buffer, xread);
				}
				if (wrote < xread) {
					bb_perror_msg(bb_msg_write_error);
					break;
				}
				total += wrote;
			} else if (xread < 0) {
				bb_perror_msg(bb_msg_read_error);
				break;
			} else if (xread == 0) {
				/* All done. */
				status = 0;
				break;
			}
		}
		RELEASE_CONFIG_BUFFER(buffer);
	}

	if (status == 0 || total)
		return total;
	/* Some sortof error occured */
	return -1;
}


extern int bb_copyfd_size(int fd1, int fd2, const off_t size)
{
	if (size) {
		return(bb_full_fd_action(fd1, fd2, size));
	}
	return(0);
}

extern int bb_copyfd_eof(int fd1, int fd2)
{
	return(bb_full_fd_action(fd1, fd2, 0));
}
