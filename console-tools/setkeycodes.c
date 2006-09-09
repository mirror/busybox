/* vi: set sw=4 ts=4: */
/*
 * setkeycodes
 *
 * Copyright (C) 1994-1998 Andries E. Brouwer <aeb@cwi.nl>
 *
 * Adjusted for BusyBox by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "busybox.h"


/* From <linux/kd.h> */
struct kbkeycode {
	unsigned int scancode, keycode;
};
enum {
	KDSETKEYCODE = 0x4B4D  /* write kernel keycode table entry */
};

extern int
setkeycodes_main(int argc, char** argv)
{
	char *ep;
	int fd, sc;
	struct kbkeycode a;

	if (argc % 2 != 1 || argc < 2) {
		bb_show_usage();
	}

	fd = get_console_fd();

	while (argc > 2) {
		a.keycode = atoi(argv[2]);
		a.scancode = sc = strtol(argv[1], &ep, 16);
		if (*ep) {
			bb_error_msg_and_die("error reading SCANCODE: '%s'", argv[1]);
		}
		if (a.scancode > 127) {
			a.scancode -= 0xe000;
			a.scancode += 128;
		}
		if (a.scancode > 255 || a.keycode > 127) {
			bb_error_msg_and_die("SCANCODE or KEYCODE outside bounds");
		}
		if (ioctl(fd,KDSETKEYCODE,&a)) {
			bb_perror_msg_and_die("failed to set SCANCODE %x to KEYCODE %d", sc, a.keycode);
		}
		argc -= 2;
		argv += 2;
	}
	return EXIT_SUCCESS;
}
