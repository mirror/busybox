/*
 *  Copyright (C) 2003 by Glenn McGrath <bug1@optushome.com.au>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *
 * TODO: -d option, need a way of recursively making directories and changing
 *           owner/group, will probably modify bb_make_directory(...)
 *       Use bb_getopt_ulflags(...) ?
 *
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libbb.h"

extern int install_main(int argc, char **argv)
{
	struct stat statbuf;
	int i;
	int ret = EXIT_SUCCESS;
	uid_t uid = -1;
	gid_t gid = -1;
	int copy_flags = 0;
	int strip_flag = 0;
	int dir_flag = 0;
	mode_t mode = 0755;

	/* -c exists for backwards compatability, its needed */
	while ((i = getopt(argc, argv, "cdg:m:o:ps")) != -1) {
		switch (i) {
		case 'd':	/* Create directories */
			dir_flag = 1;
			break;
		case 'g':	/* group */
			gid = get_ug_id(optarg, my_getgrnam);
			break;
		case 'm':	/* mode */
			bb_parse_mode(optarg, &mode);
			break;
		case 'o':	/* owner */
			uid = get_ug_id(optarg, my_getpwnam);
			break;
		case 'p':	/* preserve access and modification time, this is GNU behaviour, BSD only preserves modification time */
			copy_flags |= FILEUTILS_PRESERVE_STATUS;
			break;
		case 's':	/* Strip binaries */
			strip_flag = 1;
			/* Fall through */
		case 'c':
			/* do nothing */
			break;
		default:
			bb_show_usage();
		}
	}

	if (dir_flag) {
		for (argv += optind; *argv; argv++) {
			unsigned char *dir_name = *argv;
			unsigned char *argv_ptr;

			ret |= bb_make_directory(dir_name, mode, FILEUTILS_RECUR);
			do {
				argv_ptr = strrchr(dir_name, '/');

				/* Skip the "." and ".." directories */
				if ((dir_name[0] == '.') && ((dir_name[1] == '\0') || ((dir_name[1] == '.') && (dir_name[2] == '\0')))) {
					break;
				}
				if (chown(dir_name, uid, gid) == -1) {
					bb_perror_msg("cannot change ownership of %s", argv_ptr);
					ret |= EXIT_FAILURE;
				}
				if (argv_ptr) {
					*argv_ptr = '\0';
				}
			} while (argv_ptr);
		}
		return(ret);
	}

	if ((stat(argv[argc - 1], &statbuf) == -1) && (errno != ENOENT)) {
		bb_perror_msg_and_die("stat failed for %s: ", argv[argc - 1]);
	}

	for (i = optind; i < argc - 1; i++) {
		unsigned char *dest;

		if (S_ISDIR(statbuf.st_mode)) {
			dest = concat_path_file(argv[argc - 1], argv[i]);
		} else {
			dest = argv[argc - 1];
		}
		ret |= copy_file(argv[i], dest, copy_flags);

		/* Set the file mode */
		if (chmod(dest, mode) == -1) {
			bb_perror_msg("cannot change permissions of %s", dest);
			ret |= EXIT_FAILURE;
		}

		/* Set the user and group id */
		if (chown(dest, uid, gid) == -1) {
			bb_perror_msg("cannot change ownership of %s", dest);
			ret |= EXIT_FAILURE;			
		}
		if (strip_flag) {
			if (execlp("strip", "strip", dest, NULL) == -1) {
				bb_error_msg("strip failed");
				ret |= EXIT_FAILURE;			
			}
		}
	}
	
	return(ret);
}
