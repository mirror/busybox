/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
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

#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "busybox.h"

#if BUFSIZ < 4096
#undef BUFSIZ
#define BUFSIZ 4096
#endif

/* If chunksize is 0 copy until EOF */
extern int bb_copyfd(int fd1, int fd2, const off_t chunksize)
{
	ssize_t nread;
	size_t size;
	off_t remaining;
	RESERVE_CONFIG_BUFFER(buffer,BUFSIZ);

	remaining = size = BUFSIZ;
	if (chunksize) {
		remaining = chunksize;
	}

	do {
		if (size > remaining) {
			size = remaining;
		}

		if ((nread = safe_read(fd1, buffer, size)) > 0) {
			if (bb_full_write(fd2, buffer, nread) < 0) {
				bb_perror_msg(bb_msg_write_error);	/* match Read error below */
				break;
			}
			if (chunksize && ((remaining -= nread) == 0)) {
				return 0;
			}
		} else if (!nread) {
			if (chunksize) {
				bb_error_msg("Unable to read all data");
				break;
			}
			return 0;
		} else {				/* nread < 0 */
			bb_perror_msg("Read error");	/* match bb_msg_write_error above */
			break;
		}

	} while (1);

	return -1;
}
