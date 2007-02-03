/* vi: set sw=4 ts=4: */
/*
 * Mini chvt implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"

/* From <linux/vt.h> */
enum {
	VT_ACTIVATE = 0x5606,   /* make vt active */
	VT_WAITACTIVE = 0x5607  /* wait for vt active */
};

int chvt_main(int argc, char **argv);
int chvt_main(int argc, char **argv)
{
	int fd, num;

	if (argc != 2) {
		bb_show_usage();
	}

	fd = get_console_fd();
	num = xatoul_range(argv[1], 1, 63);
	if ((-1 == ioctl(fd, VT_ACTIVATE, num))
	|| (-1 == ioctl(fd, VT_WAITACTIVE, num))) {
		bb_perror_msg_and_die("ioctl");
	}
	return EXIT_SUCCESS;
}
