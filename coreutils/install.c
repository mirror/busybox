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

static const struct option install_long_options[] = {
	{ "directory",	0,	NULL,	'd' },
	{ "preserve-timestamps",	0,	NULL,	'p' },
	{ "strip",	0,	NULL,	's' },
	{ "group",	0,	NULL,	'g' },
	{ "mode", 	0,	NULL,	'm' },
	{ "owner",	0,	NULL,	'o' },
	{ 0,	0,	0,	0 }
};

extern int install_main(int argc, char **argv)
{
	struct stat statbuf;
	mode_t mode;
	uid_t uid;
	gid_t gid;
	char *gid_str = "-1";
	char *uid_str = "-1";
	char *mode_str = "0755";
	int copy_flags = FILEUTILS_DEREFERENCE | FILEUTILS_FORCE;
	int ret = EXIT_SUCCESS;
	int flags;
	int i;

	bb_applet_long_options = install_long_options;
	bb_opt_complementaly = "s~d:d~s";
	/* -c exists for backwards compatability, its needed */
	flags = bb_getopt_ulflags(argc, argv, "cdpsg:m:o:", &gid_str, &mode_str, &uid_str);	/* 'a' must be 2nd */

	/* Check valid options were given */
	if(flags & 0x80000000UL) {
		bb_show_usage();
	}

	/* preserve access and modification time, this is GNU behaviour, BSD only preserves modification time */
	if (flags & INSTALL_OPT_PRESERVE_TIME) {
		copy_flags |= FILEUTILS_PRESERVE_STATUS;
	}
	bb_parse_mode(mode_str, &mode);
	gid = get_ug_id(gid_str, my_getgrnam);
	uid = get_ug_id(uid_str, my_getpwnam);
	umask(0);

	/* Create directories
	 * dont use bb_make_directory() as it cant change uid or gid
	 * perhaps bb_make_directory() should be improved.
	 */
	if (flags & INSTALL_OPT_DIRECTORY) {
		for (argv += optind; *argv; argv++) {
			char *old_argv_ptr = *argv + 1;
			char *argv_ptr;
			do {
				argv_ptr = strchr(old_argv_ptr, '/');
				old_argv_ptr = argv_ptr;
				if (argv_ptr) {
					*argv_ptr = '\0';
					old_argv_ptr++;
				}
				if (mkdir(*argv, mode) == -1) {
					if (errno != EEXIST) {
						bb_perror_msg("coulnt create %s", *argv);
						ret = EXIT_FAILURE;
						break;
					}
				}
				else if (lchown(*argv, uid, gid) == -1) {
					bb_perror_msg("cannot change ownership of %s", *argv);
					ret = EXIT_FAILURE;
					break;
				}
				if (argv_ptr) {
					*argv_ptr = '/';
				}
			} while (old_argv_ptr);
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
			ret = EXIT_FAILURE;
		}

		/* Set the user and group id */
		if (lchown(dest, uid, gid) == -1) {
			bb_perror_msg("cannot change ownership of %s", dest);
			ret = EXIT_FAILURE;
		}
		if (flags & INSTALL_OPT_STRIP) {
			if (execlp("strip", "strip", dest, NULL) == -1) {
				bb_error_msg("strip failed");
				ret = EXIT_FAILURE;
			}
		}
	}

	return(ret);
}
