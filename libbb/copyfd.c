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


extern size_t copyfd(int fd1, int fd2)
{
	char buf[32768], *writebuf;
	int status = TRUE;
	size_t totalread = 0, bytesread, byteswritten;

	while(status) {
		bytesread = read(fd1, &buf, sizeof(buf));
		if(bytesread == -1) {
			error_msg("read: %s", strerror(errno));
			status = FALSE;
			break;
		}
		byteswritten = 0;
		writebuf = buf;
		while(bytesread) {
			byteswritten = write( fd2, &writebuf, bytesread );
			if(byteswritten == -1) {
				error_msg("write: %s", strerror(errno));
				status = FALSE;
				break;
			}
			bytesread -= byteswritten;
			writebuf += byteswritten;
		}
	}
	if ( status == TRUE )
		return totalread;
	else
		return -1;
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
