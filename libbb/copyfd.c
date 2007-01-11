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

#include "libbb.h"


#if BUFSIZ < 4096
#undef BUFSIZ
#define BUFSIZ 4096
#endif


static off_t bb_full_fd_action(int src_fd, int dst_fd, off_t size)
{
	int status = -1;
	off_t total = 0;
	RESERVE_CONFIG_BUFFER(buffer, BUFSIZ);

	if (src_fd < 0) goto out;

	if (!size) {
		size = BUFSIZ;
		status = 1; /* copy until eof */
	}

	while (1) {
		ssize_t rd;

		rd = safe_read(src_fd, buffer, size > BUFSIZ ? BUFSIZ : size);

		if (!rd) { /* eof - all done */
			status = 0;
			break;
		}
		if (rd < 0) {
			bb_perror_msg(bb_msg_read_error);
			break;
		}
		/* dst_fd == -1 is a fake, else... */
		if (dst_fd >= 0) {
			ssize_t wr = full_write(dst_fd, buffer, rd);
			if (wr < rd) {
				bb_perror_msg(bb_msg_write_error);
				break;
			}
		}
		total += rd;
		if (status < 0) { /* if we aren't copying till EOF... */
			size -= rd;
			if (!size) {
				/* 'size' bytes copied - all done */
				status = 0;
				break;
			}
		}
	}
 out:
	RELEASE_CONFIG_BUFFER(buffer);
	return status ? -1 : total;
}


#if 0
void complain_copyfd_and_die(off_t sz)
{
	if (sz != -1)
		bb_error_msg_and_die("short read");
	/* if sz == -1, bb_copyfd_XX already complained */
	exit(xfunc_error_retval);
}
#endif

off_t bb_copyfd_size(int fd1, int fd2, off_t size)
{
	if (size) {
		return bb_full_fd_action(fd1, fd2, size);
	}
	return 0;
}

void bb_copyfd_exact_size(int fd1, int fd2, off_t size)
{
	off_t sz = bb_copyfd_size(fd1, fd2, size);
	if (sz == size)
		return;
	if (sz != -1)
		bb_error_msg_and_die("short read");
	/* if sz == -1, bb_copyfd_XX already complained */
	exit(xfunc_error_retval);
}

off_t bb_copyfd_eof(int fd1, int fd2)
{
	return bb_full_fd_action(fd1, fd2, 0);
}
