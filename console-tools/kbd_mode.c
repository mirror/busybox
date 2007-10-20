/* vi: set sw=4 ts=4: */
/*
 * Mini loadkmap implementation for busybox
 *
 * Copyright (C) 2007 Loïc Grenié <loic.grenie@gmail.com>
 *   written using Andries Brouwer <aeb@cwi.nl>'s kbd_mode from
 *   console-utils v0.2.3, licensed under GNU GPLv2
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 */

#include <getopt.h>
#include "libbb.h"
#include <linux/kd.h>

int kbd_mode_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int kbd_mode_main(int argc, char **argv)
{
	static const char opts[] = "saku";

	const char *opt = argv[1];
	const char *p;
	int fd;

	fd = get_console_fd();
	if (fd < 0) /* get_console_fd() already complained */
		return EXIT_FAILURE;

	if (opt == NULL) {
		/* No arg */
		const char *msg = "unknown";
		int mode;

		ioctl(fd, KDGKBMODE, &mode);
		switch(mode) {
		case K_RAW:
			msg = "raw (scancode)";
			break;
		case K_XLATE:
			msg = "default (ASCII)";
			break;
		case K_MEDIUMRAW:
			msg = "mediumraw (keycode)";
			break;
		case K_UNICODE:
			msg = "Unicode (UTF-8)";
			break;
		}
		printf("The keyboard is in %s mode\n", msg);
	}
	else if (argc > 2 /* more than 1 arg */
	 || *opt != '-' /* not an option */
	 || (p = strchr(opts, opt[1])) == NULL /* not an option we expect */
	 || opt[2] != '\0' /* more than one option char */
	) {
		bb_show_usage();
		/* return EXIT_FAILURE; - not reached */
	}
	else {
#if K_RAW != 0 || K_XLATE != 1 || K_MEDIUMRAW != 2 || K_UNICODE != 3
#error kbd_mode must be changed
#endif
		/* The options are in the order of the various K_xxx */
		ioctl(fd, KDSKBMODE, p - opts);
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		close(fd);
	return EXIT_SUCCESS;
}
