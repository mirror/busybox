/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
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

/* Grumble...  The 2.6.x kernel breaks asm/posix_types.h
 * so we get to try and cope as best we can... */
#include <linux/version.h>
#include <asm/posix_types.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define __bb_kernel_dev_t   __kernel_old_dev_t
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
#define __bb_kernel_dev_t   __kernel_dev_t
#else
#define __bb_kernel_dev_t   unsigned short
#endif

/* Stuff stolen from linux/loop.h */
#define LO_NAME_SIZE        64
#define LO_KEY_SIZE         32
#define LOOP_SET_FD         0x4C00
#define LOOP_CLR_FD         0x4C01
#define LOOP_SET_STATUS     0x4C02
#define LOOP_GET_STATUS     0x4C03
struct loop_info {
	int                lo_number;
	__bb_kernel_dev_t  lo_device;
	unsigned long      lo_inode;
	__bb_kernel_dev_t  lo_rdevice;
	int                lo_offset;
	int                lo_encrypt_type;
	int                lo_encrypt_key_size;
	int                lo_flags;
	char               lo_name[LO_NAME_SIZE];
	unsigned char      lo_encrypt_key[LO_KEY_SIZE];
	unsigned long      lo_init[2];
	char               reserved[4];
};

extern int del_loop(const char *device)
{
	int fd;

	if ((fd = open(device, O_RDONLY)) < 0) {
		bb_perror_msg("%s", device);
		return (FALSE);
	}
	if (ioctl(fd, LOOP_CLR_FD, 0) < 0) {
		close(fd);
		bb_perror_msg("ioctl: LOOP_CLR_FD");
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
		bb_perror_msg("%s", file);
		return 1;
	}
	if ((fd = open(device, mode)) < 0) {
		close(ffd);
		bb_perror_msg("%s", device);
		return 1;
	}
	*loopro = (mode == O_RDONLY);

	memset(&loopinfo, 0, sizeof(loopinfo));
	safe_strncpy(loopinfo.lo_name, file, LO_NAME_SIZE);

	loopinfo.lo_offset = offset;

	loopinfo.lo_encrypt_key_size = 0;
	if (ioctl(fd, LOOP_SET_FD, ffd) < 0) {
		bb_perror_msg("ioctl: LOOP_SET_FD");
		close(fd);
		close(ffd);
		return 1;
	}
	if (ioctl(fd, LOOP_SET_STATUS, &loopinfo) < 0) {
		(void) ioctl(fd, LOOP_CLR_FD, 0);
		bb_perror_msg("ioctl: LOOP_SET_STATUS");
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
