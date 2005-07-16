/* vi: set sw=4 ts=4: */
/*
 * Mini cp implementation for busybox
 *
 *
 * Copyright (C) 2000 by Matt Kraai <kraai@alumni.carnegiemellon.edu>
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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>

#include "busybox.h"

extern int cp_main(int argc, char **argv)
{
	int status = 0;
	int opt;
	int flags = 0;
	int i;

	while ((opt = getopt(argc, argv, "adfipRr")) != -1)
		switch (opt) {
		case 'a':
			flags |= FILEUTILS_PRESERVE_STATUS | FILEUTILS_RECUR;
			/* fallthrough */
		case 'd':
			flags |= FILEUTILS_PRESERVE_SYMLINKS;
			break;
		case 'f':
			flags |= FILEUTILS_FORCE;
			break;
		case 'i':
			flags |= FILEUTILS_INTERACTIVE;
			break;
		case 'p':
			flags |= FILEUTILS_PRESERVE_STATUS;
			break;
		case 'R':
			flags |= FILEUTILS_RECUR | FILEUTILS_PRESERVE_STATUS;
			break;
		case 'r':
			flags |= FILEUTILS_RECUR;
			break;
		default:
			show_usage();
		}
	
	if (optind + 2 > argc)
		show_usage();

	/* If there are only two arguments and...  */
	if (optind + 2 == argc) {
		struct stat source_stat;
		struct stat dest_stat;
		int source_exists = 1;
		int dest_exists = 1;

		if (((flags & FILEUTILS_PRESERVE_SYMLINKS) &&
				lstat(argv[optind], &source_stat) < 0) ||
				(!(flags & FILEUTILS_PRESERVE_SYMLINKS) &&
				 stat(argv[optind], &source_stat))) {
			if (errno != ENOENT)
				perror_msg_and_die("unable to stat `%s'", argv[optind]);
			source_exists = 0;
		}

		if (stat(argv[optind + 1], &dest_stat) < 0) {
			if (errno != ENOENT)
				perror_msg_and_die("unable to stat `%s'", argv[optind + 1]);
			dest_exists = 0;
		}
		
		/* ...if neither is a directory or...  */
		if (((!source_exists || !S_ISDIR(source_stat.st_mode)) &&
				(!dest_exists || !S_ISDIR(dest_stat.st_mode))) ||
				/* ...recursing, the first is a directory, and the
				 * second doesn't exist, then... */
				((flags & FILEUTILS_RECUR) && S_ISDIR(source_stat.st_mode) &&
				 !dest_exists)) {
			/* ...do a simple copy.  */
			if (copy_file(argv[optind], argv[optind + 1], flags) < 0)
				status = 1;
			return status;
		}
	}

	for (i = optind; i < argc - 1; i++) {
		char *dest = concat_path_file(argv[argc - 1],
				get_last_path_component(argv[i]));
		if (copy_file(argv[i], dest, flags) < 0)
			status = 1;
		free(dest);
	}

	return status;
}
