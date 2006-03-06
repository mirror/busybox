/* vi: set sw=4 ts=4: */
/*
 * Mini ln implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
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
/* BB_AUDIT GNU options missing: -d, -F, -i, and -v. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/ln.html */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "busybox.h"

#define LN_SYMLINK          1
#define LN_FORCE            2
#define LN_NODEREFERENCE    4
#define LN_BACKUP           8
#define LN_SUFFIX           16

int ln_main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;
	int flag;
	char *last;
	char *src_name;
	char *src;
	char *suffix = "~";
	struct stat statbuf;
	int (*link_func)(const char *, const char *);

	flag = bb_getopt_ulflags(argc, argv, "sfnbS:", &suffix);

	if (argc == optind) {
		bb_show_usage();
	}

	last = argv[argc - 1];
	argv += optind;

	if (argc == optind + 1) {
		*--argv = last;
		last = bb_get_last_path_component(bb_xstrdup(last));
	}

	do {
		src_name = NULL;
		src = last;

		if (is_directory(src,
						 (flag & LN_NODEREFERENCE) ^ LN_NODEREFERENCE,
						 NULL)) {
			src_name = bb_xstrdup(*argv);
			src = concat_path_file(src, bb_get_last_path_component(src_name));
			free(src_name);
			src_name = src;
		}
		if (!(flag & LN_SYMLINK) && stat(*argv, &statbuf)) {
			bb_perror_msg("%s", *argv);
			status = EXIT_FAILURE;
			free(src_name);
			continue;
		}

		if (flag & LN_BACKUP) {
				char *backup;
				backup = bb_xasprintf("%s%s", src, suffix);
				if (rename(src, backup) < 0 && errno != ENOENT) {
						bb_perror_msg("%s", src);
						status = EXIT_FAILURE;
						free(backup);
						continue;
				}
				free(backup);
				/*
				 * When the source and dest are both hard links to the same
				 * inode, a rename may succeed even though nothing happened.
				 * Therefore, always unlink().
				 */
				unlink(src);
		} else if (flag & LN_FORCE) {
			unlink(src);
		}

		link_func = link;
		if (flag & LN_SYMLINK) {
			link_func = symlink;
		}

		if (link_func(*argv, src) != 0) {
			bb_perror_msg("%s", src);
			status = EXIT_FAILURE;
		}

		free(src_name);

	} while ((++argv)[1]);

	return status;
}
