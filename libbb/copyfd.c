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
#include "libbb.h"

/* If chunksize is 0 copy untill EOF */
extern int copyfd(int fd1, int fd2, const off_t chunksize)
{
	size_t nread;
	size_t nwritten;
	size_t size;
	size_t remaining;
	char buffer[BUFSIZ];

	if (chunksize) {
		remaining = chunksize;
	} else {
		remaining = -1;
	}

	do {
		if ((chunksize > BUFSIZ) || (chunksize == 0)) {
			size = BUFSIZ;
		} else {
			size = chunksize;
		}

		nread = safe_read(fd1, buffer, size);

		if (nread == -1) {
			perror_msg("read failure");
			return(-1);
		}
		else if (nread == 0) {
			if (chunksize) {
				error_msg("Unable to read all data");
				return(-1);
			} else {
				return(0);
			}
		}

		nwritten = full_write(fd2, buffer, nread);

		if (nwritten != nread) {
			error_msg("Unable to write all data");
			return(-1);
		}

		if (chunksize) {
			remaining -= nwritten;
		}
	} while (remaining != 0);

	return 0;
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
