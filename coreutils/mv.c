/* vi: set sw=4 ts=4: */
/*
 * Mini mv implementation for busybox
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

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Size reduction and improved error checking.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <getopt.h>
#include "busybox.h"
#include "libcoreutils/coreutils.h"

static const struct option mv_long_options[] = {
	{ "interactive", 0, NULL, 'i' },
	{ "force", 0, NULL, 'f' },
	{ 0, 0, 0, 0 }
};

#define OPT_FILEUTILS_FORCE       1
#define OPT_FILEUTILS_INTERACTIVE 2

static const char fmt[] = "cannot overwrite %sdirectory with %sdirectory";

extern int mv_main(int argc, char **argv)
{
	struct stat dest_stat;
	const char *last;
	const char *dest;
	unsigned long flags;
	int dest_exists;
	int status = 0;

	bb_applet_long_options = mv_long_options;
	bb_opt_complementaly = "f-i:i-f";
	flags = bb_getopt_ulflags(argc, argv, "fi");
	if (optind + 2 > argc) {
		bb_show_usage();
	}

	last = argv[argc - 1];
	argv += optind;

	if (optind + 2 == argc) {
		if ((dest_exists = cp_mv_stat(last, &dest_stat)) < 0) {
			return 1;
		}

		if (!(dest_exists & 2)) {
			dest = last;
			goto DO_MOVE;
		}
	}
	
	do {
		dest = concat_path_file(last, bb_get_last_path_component(*argv));

		if ((dest_exists = cp_mv_stat(dest, &dest_stat)) < 0) {
			goto RET_1;
		}

DO_MOVE:
		
		if (dest_exists && !(flags & OPT_FILEUTILS_FORCE) &&
			((access(dest, W_OK) < 0 && isatty(0)) ||
			(flags & OPT_FILEUTILS_INTERACTIVE))) {
			if (fprintf(stderr, "mv: overwrite `%s'? ", dest) < 0) {
				goto RET_1;	/* Ouch! fprintf failed! */
			}
			if (!bb_ask_confirmation()) {
				goto RET_0;
			}
		}
		if (rename(*argv, dest) < 0) {
			struct stat source_stat;
			int source_exists;

			if (errno != EXDEV) {
				bb_perror_msg("unable to rename `%s'", *argv);
			}
			else if ((source_exists = cp_mv_stat(*argv, &source_stat)) >= 0) {
				if (dest_exists) {
					if (dest_exists == 3) {
						if (source_exists != 3) {
							bb_error_msg(fmt, "", "non-");
							goto RET_1;
						}
					} else {
						if (source_exists == 3) {
							bb_error_msg(fmt, "non-", "");
							goto RET_1;
						}
					}
					if (unlink(dest) < 0) {
						bb_perror_msg("cannot remove `%s'", dest);
						goto RET_1;
					}
				}			
				if ((copy_file(*argv, dest,
					FILEUTILS_RECUR | FILEUTILS_PRESERVE_STATUS) >= 0) &&
					(remove_file(*argv, FILEUTILS_RECUR | FILEUTILS_FORCE) >= 0)) {
					goto RET_0;
				}
			}
RET_1:
			status = 1;
		}
RET_0:
		if (dest != last) {
			free((void *) dest);
		}	
	} while (*++argv != last);

	return (status);
}
