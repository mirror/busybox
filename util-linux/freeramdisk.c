/* vi: set sw=4 ts=4: */
/*
 * freeramdisk and fdflush implementations for busybox
 *
 * Copyright (C) 2000 and written by Emanuele Caratti <wiz@iol.it>
 * Adjusted a bit by Erik Andersen <andersen@codepoet.org>
 * Unified with fdflush by Tito Ragusa <farmatito@tiscali.it>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <unistd.h>
#include "busybox.h"

/* From linux/fs.h */
#define BLKFLSBUF  _IO(0x12,97)
/* From <linux/fd.h> */
#define FDFLUSH  _IO(2,0x4b)

int freeramdisk_main(int argc, char **argv)
{
	int result;
	int fd;

	if (argc != 2) bb_show_usage();

	fd = bb_xopen(argv[1], O_RDWR);

	// Act like freeramdisk, fdflush, or both depending on configuration.
	result = ioctl(fd, (ENABLE_FREERAMDISK && bb_applet_name[1]=='r')
			|| !ENABLE_FDFLUSH ? BLKFLSBUF : FDFLUSH);

	if (ENABLE_FEATURE_CLEAN_UP) close(fd);

	if (result) bb_perror_msg_and_die("%s", argv[1]);
	return EXIT_SUCCESS;
}
