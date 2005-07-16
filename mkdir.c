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

#include <errno.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "busybox.h"

extern int mkdir_main (int argc, char **argv)
{
	mode_t mode = -1;
	int flags = 0;
	int status = 0;
	int i, opt;

	while ((opt = getopt (argc, argv, "m:p")) != -1) {
		switch (opt) {
		case 'm':
			mode = 0777;
			if (!parse_mode (optarg, &mode))
				error_msg_and_die ("invalid mode `%s'", optarg);
			break;
		case 'p':
			flags |= FILEUTILS_RECUR;
			break;
		default:
			show_usage ();
		}
	}

	if (optind == argc)
		show_usage ();

	for (i = optind; i < argc; i++)
		if (make_directory (argv[i], mode, flags) < 0)
			status = 1;

	return status;
}
