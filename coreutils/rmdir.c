/* vi: set sw=4 ts=4: */
/*
 * rmdir implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/rmdir.html */

#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include "busybox.h"

int rmdir_main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;
	int flags;
	int do_dot;
	char *path;

	flags = getopt32(argc, argv, "p");

	argv += optind;

	if (!*argv) {
		bb_show_usage();
	}

	do {
		path = *argv;

		/* Record if the first char was a '.' so we can use dirname later. */
		do_dot = (*path == '.');

		do {
			if (rmdir(path) < 0) {
				bb_perror_msg("'%s'", path);	/* Match gnu rmdir msg. */
				status = EXIT_FAILURE;
			} else if (flags) {
				/* Note: path was not empty or null since rmdir succeeded. */
				path = dirname(path);
				/* Path is now just the parent component.  Note that dirname
				 * returns "." if there are no parents.  We must distinguish
				 * this from the case of the original path starting with '.'.
				 */
				if (do_dot || (*path != '.') || path[1]) {
					continue;
				}
			}
			break;
		} while (1);

	} while (*++argv);

	return status;
}
