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
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "busybox.h"
#include "libcoreutils/coreutils.h"

#define INSTALL_OPT_CMD	1
#define INSTALL_OPT_DIRECTORY	2
#define INSTALL_OPT_PRESERVE_TIME	4
#define INSTALL_OPT_STRIP	8
#define INSTALL_OPT_GROUP  16
#define INSTALL_OPT_MODE  32
#define INSTALL_OPT_OWNER  64

extern int install_main(int argc, char **argv)
{
	struct stat statbuf;
	mode_t mode = 0755;
	uid_t uid = -1;
	gid_t gid = -1;
	char *gid_str;
	char *uid_str;
	char *mode_str;
	int copy_flags = FILEUTILS_DEREFERENCE | FILEUTILS_FORCE;
	int ret = EXIT_SUCCESS;
	int flags;
	int i;

	/* -c exists for backwards compatability, its needed */
	flags = bb_getopt_ulflags(argc, argv, "cdpsg:m:o:", &gid_str, &mode_str, &uid_str);	/* 'a' must be 2nd */

	/* preserve access and modification time, this is GNU behaviour, BSD only preserves modification time */
	if (flags & INSTALL_OPT_PRESERVE_TIME) {
		copy_flags |= FILEUTILS_PRESERVE_STATUS;
	}
	if (flags & INSTALL_OPT_GROUP) {
		gid = get_ug_id(gid_str, my_getgrnam);
	}
	if (flags & INSTALL_OPT_MODE) {
		bb_parse_mode(mode_str, &mode);
	}
	if (flags & INSTALL_OPT_OWNER) {
		uid = get_ug_id(uid_str, my_getpwnam);
	}

	/* Create directories */
	if (flags & INSTALL_OPT_DIRECTORY) {
	
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
				if (lchown(dir_name, uid, gid) == -1) {
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
	
	cp_mv_stat2(argv[argc - 1], &statbuf, lstat);
	for (i = optind; i < argc - 1; i++) {
		unsigned char *dest;

		if (S_ISDIR(statbuf.st_mode)) {
			dest = concat_path_file(argv[argc - 1], basename(argv[i]));
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
		if (lchown(dest, uid, gid) == -1) {
			bb_perror_msg("cannot change ownership of %s", dest);
			ret |= EXIT_FAILURE;			
		}
		if (flags & INSTALL_OPT_STRIP) {
			if (execlp("strip", "strip", dest, NULL) == -1) {
				bb_error_msg("strip failed");
				ret |= EXIT_FAILURE;			
			}
		}
	}
	
	return(ret);
}
