/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2001 Erik Andersen <andersee@debian.org>
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


extern int copyfd(int fd1, int fd2)
{
	char buf[8192];
	ssize_t nread, nwrote;

	while (1) {
		nread = safe_read(fd1, buf, sizeof(buf));
		if (nread == 0)
			break;
		if (nread == -1) {
			perror_msg("read");
			return -1;
		}

		nwrote = full_write(fd2, buf, nread);
		if (nwrote == -1) {
			perror_msg("write");
			return -1;
		}
	}

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
