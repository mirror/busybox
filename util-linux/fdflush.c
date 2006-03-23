/* vi: set sw=4 ts=4: */
/*
 * Mini fdflush implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@perens.com>.
 * Copyright (C) 2003 by Erik Andersen <andersen@codeoet.org>
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
 *
 */

#include <stdio.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include "busybox.h"

/* From <linux/fd.h> */
#define FDFLUSH  _IO(2,0x4b)

extern int fdflush_main(int argc, char **argv)
{
	int fd, result;

	if (argc <= 1)
		bb_show_usage();

	fd = bb_xopen(argv[1], 0);

	result = ioctl(fd, FDFLUSH, 0);
#ifdef CONFIG_FEATURE_CLEAN_UP
	close(fd);
#endif
	if (result) {
		bb_perror_nomsg_and_die();
	}

	/* Don't bother closing.  Exit does
	 * that, so we can save a few bytes */
	return EXIT_SUCCESS;
}
