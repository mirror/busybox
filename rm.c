/* vi: set sw=4 ts=4: */
/*
 * Mini rm implementation for busybox
 *
 *
 * Copyright (C) 2001 Matt Kraai <kraai@alumni.carnegiemellon.edu>
 *
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
#include <time.h>
#include <utime.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "busybox.h"

extern int rm_main(int argc, char **argv)
{
	int status = 0;
	int opt;
	int flags = 0;
	int i;

	while ((opt = getopt(argc, argv, "fiRr")) != -1) {
		switch (opt) {
		case 'f':
			flags &= ~FILEUTILS_INTERACTIVE;
			flags |= FILEUTILS_FORCE;
			break;
		case 'i':
			flags &= ~FILEUTILS_FORCE;
			flags |= FILEUTILS_INTERACTIVE;
			break;
		case 'R':
		case 'r':
			flags |= FILEUTILS_RECUR;
			break;
		}
	}

	if (!(flags & FILEUTILS_FORCE) && optind == argc)
		show_usage();

	for (i = optind; i < argc; i++) {
		char *base = get_last_path_component(argv[i]);

		if (strcmp(base, ".") == 0 || strcmp(base, "..") == 0) {
			error_msg("cannot remove `.' or `..'");
			status = 1;
			continue;
		}

		if (remove_file(argv[i], flags) < 0)
			status = 1;
	}

	return status;
}
