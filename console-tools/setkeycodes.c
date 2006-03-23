/* vi: set sw=4 ts=4: */
/*
 * setkeycodes
 *
 * Copyright (C) 1994-1998 Andries E. Brouwer <aeb@cwi.nl>
 *
 * Adjusted for BusyBox by Erik Andersen <andersen@codepoet.org>
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
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "busybox.h"


/* From <linux/kd.h> */
struct kbkeycode {
	unsigned int scancode, keycode;
};
static const int KDSETKEYCODE = 0x4B4D;  /* write kernel keycode table entry */

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
	    perror("KDSETKEYCODE");
		bb_error_msg_and_die("failed to set SCANCODE %x to KEYCODE %d", sc, a.keycode);
	}
	argc -= 2;
	argv += 2;
    }
	return EXIT_SUCCESS;
}
