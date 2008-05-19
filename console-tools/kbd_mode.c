/* vi: set sw=4 ts=4: */
/*
 * Mini kbd_mode implementation for busybox
 *
 * Copyright (C) 2007 Loïc Grenié <loic.grenie@gmail.com>
 *   written using Andries Brouwer <aeb@cwi.nl>'s kbd_mode from
 *   console-utils v0.2.3, licensed under GNU GPLv2
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 */

#include "libbb.h"
#include <linux/kd.h>

int kbd_mode_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int kbd_mode_main(int ATTRIBUTE_UNUSED argc, char **argv)
{
	int fd;
	unsigned opt;
	enum {
		SCANCODE = (1<<0),
		ASCII	 = (1<<1),
		MEDIUMRAW= (1<<2),
		UNICODE	 = (1<<3)
	};
	static const char KD_xxx[] ALIGN1 = "saku";
	opt = getopt32(argv, KD_xxx);
	fd = get_console_fd();
/*	if (fd < 0)
		return EXIT_FAILURE;
*/
	if (!opt) { /* print current setting */
		const char *mode = "unknown";
		int m;

		ioctl(fd, KDGKBMODE, &m);
		if (m == K_RAW)
			mode = "raw (scancode)";
		else if (m == K_XLATE)
			mode = "default (ASCII)";
		else if (m == K_MEDIUMRAW)
			mode = "mediumraw (keycode)";
		else if (m == K_UNICODE)
			mode = "Unicode (UTF-8)";
		printf("The keyboard is in %s mode\n", mode);
	} else {
		opt = opt & UNICODE ? 3 : opt >> 1;
		xioctl(fd, KDSKBMODE, &opt);
	}

	if (ENABLE_FEATURE_CLEAN_UP)
		close(fd);
	return EXIT_SUCCESS;
}
