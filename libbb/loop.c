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

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "libbb.h"
#include "loop.h" /* Pull in loop device support */

extern int del_loop(const char *device)
{
	int fd;

	if ((fd = open(device, O_RDONLY)) < 0) {
		perror_msg("%s", device);
		return (FALSE);
	}
	if (ioctl(fd, LOOP_CLR_FD, 0) < 0) {
		perror_msg("ioctl: LOOP_CLR_FD");
		return (FALSE);
	}
	close(fd);
	return (TRUE);
}

extern int set_loop(const char *device, const char *file, int offset,
					int *loopro)
{
	struct loop_info loopinfo;
	int fd, ffd, mode;

	mode = *loopro ? O_RDONLY : O_RDWR;
	if ((ffd = open(file, mode)) < 0 && !*loopro
		&& (errno != EROFS || (ffd = open(file, mode = O_RDONLY)) < 0)) {
		perror_msg("%s", file);
		return 1;
	}
	if ((fd = open(device, mode)) < 0) {
		close(ffd);
		perror_msg("%s", device);
		return 1;
	}
	*loopro = (mode == O_RDONLY);

	memset(&loopinfo, 0, sizeof(loopinfo));
	safe_strncpy(loopinfo.lo_name, file, LO_NAME_SIZE);

	loopinfo.lo_offset = offset;

	loopinfo.lo_encrypt_key_size = 0;
	if (ioctl(fd, LOOP_SET_FD, ffd) < 0) {
		perror_msg("ioctl: LOOP_SET_FD");
		close(fd);
		close(ffd);
		return 1;
	}
	if (ioctl(fd, LOOP_SET_STATUS, &loopinfo) < 0) {
		(void) ioctl(fd, LOOP_CLR_FD, 0);
		perror_msg("ioctl: LOOP_SET_STATUS");
		close(fd);
		close(ffd);
		return 1;
	}
	close(fd);
	close(ffd);
	return 0;
}

extern char *find_unused_loop_device(void)
{
	char dev[20];
	int i, fd;
	struct stat statbuf;
	struct loop_info loopinfo;

	for (i = 0; i <= 7; i++) {
		sprintf(dev, LOOP_FORMAT, i);
		if (stat(dev, &statbuf) == 0 && S_ISBLK(statbuf.st_mode)) {
			if ((fd = open(dev, O_RDONLY)) >= 0) {
				if (ioctl(fd, LOOP_GET_STATUS, &loopinfo) != 0) {
					if (errno == ENXIO) {	/* probably free */
						close(fd);
						return strdup(dev);
					}
				}
				close(fd);
			}
		}
	}
	return NULL;
}


/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
