/* vi: set sw=4 ts=4: */
/*
 *  setconsole.c - redirect system console output
 *
 *  Copyright (C) 2004,2005  Enrik Berkhan <Enrik.Berkhan@inka.de>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <getopt.h>
#include "libbb.h"

#if ENABLE_FEATURE_SETCONSOLE_LONG_OPTIONS
static const char setconsole_longopts[] ALIGN1 =
	"reset\0" No_argument "r"
	;
#endif

#define OPT_SETCONS_RESET 1

int setconsole_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int setconsole_main(int argc, char **argv)
{
	unsigned long flags;
	const char *device = CURRENT_TTY;

#if ENABLE_FEATURE_SETCONSOLE_LONG_OPTIONS
	applet_long_options = setconsole_longopts;
#endif
	flags = getopt32(argv, "r");

	if (argc - optind > 1)
		bb_show_usage();

	if (argc - optind == 1) {
		if (flags & OPT_SETCONS_RESET)
			bb_show_usage();
		device = argv[optind];
	} else {
		if (flags & OPT_SETCONS_RESET)
			device = DEV_CONSOLE;
	}

	xioctl(xopen(device, O_RDONLY), TIOCCONS, NULL);
	return EXIT_SUCCESS;
}
