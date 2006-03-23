/* vi: set sw=4 ts=4: */
/*
 * Mini mkdir implementation for busybox
 *
 * Copyright (C) 2001 Matt Kraai <kraai@alumni.carnegiemellon.edu>
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

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/mkdir.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Fixed broken permission setting when -p was used; especially in
 * conjunction with -m.
 */

#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include "busybox.h"

static const struct option mkdir_long_options[] = {
	{ "mode", 1, NULL, 'm' },
	{ "parents", 0, NULL, 'p' },
	{ 0, 0, 0, 0 }
};

extern int mkdir_main (int argc, char **argv)
{
	mode_t mode = (mode_t)(-1);
	int status = EXIT_SUCCESS;
	int flags = 0;
	unsigned long opt;
	char *smode;

	bb_applet_long_options = mkdir_long_options;
	opt = bb_getopt_ulflags(argc, argv, "m:p", &smode);
	if(opt & 1) {
			mode = 0777;
		if (!bb_parse_mode (smode, &mode)) {
			bb_error_msg_and_die ("invalid mode `%s'", smode);
		}
	}
	if(opt & 2)
		flags |= FILEUTILS_RECUR;

	if (optind == argc) {
		bb_show_usage();
	}

	argv += optind;

	do {
		if (bb_make_directory(*argv, mode, flags)) {
			status = EXIT_FAILURE;
		}
	} while (*++argv);

	return status;
}
