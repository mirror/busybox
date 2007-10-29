/* vi: set sw=4 ts=4: */
/*
 * Mini chvt implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

/* From <linux/vt.h> */
enum {
	VT_ACTIVATE = 0x5606,   /* make vt active */
	VT_WAITACTIVE = 0x5607  /* wait for vt active */
};

int chvt_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int chvt_main(int argc, char **argv)
{
	int fd, num;

	if (argc != 2) {
		bb_show_usage();
	}

	fd = get_console_fd();
	num = xatou_range(argv[1], 1, 63);
	/* double cast suppresses "cast to ptr from int of different size" */
	xioctl(fd, VT_ACTIVATE, (void *)(ptrdiff_t)num);
	xioctl(fd, VT_WAITACTIVE, (void *)(ptrdiff_t)num);
	return EXIT_SUCCESS;
}
