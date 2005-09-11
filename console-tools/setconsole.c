/* vi: set sw=4 ts=4: */
/*
 *  setconsole.c - redirect system console output
 *
 *  Copyright (C) 2004,2005  Enrik Berkhan <Enrik.Berkhan@inka.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <stdio.h>
#include <getopt.h> /* struct option */

#include "busybox.h"

static const struct option setconsole_long_options[] = {
	{ "reset", 0, NULL, 'r' },
	{ 0, 0, 0, 0 }
};

#define OPT_SETCONS_RESET 1

int setconsole_main(int argc, char **argv)
{
	unsigned long flags;
	const char *device = CURRENT_TTY;

	bb_applet_long_options = setconsole_long_options;
	flags = bb_getopt_ulflags(argc, argv, "r");

	if (argc - optind > 1)
		bb_show_usage();

	if (argc - optind == 1) {
		if (flags & OPT_SETCONS_RESET)
			bb_show_usage();
		device = argv[optind];
	} else {
		if (flags & OPT_SETCONS_RESET)
			device = CONSOLE_DEV;
	}

	if (-1 == ioctl(bb_xopen(device, O_RDONLY), TIOCCONS)) {
		bb_perror_msg_and_die("TIOCCONS");
	}
	return EXIT_SUCCESS;
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
