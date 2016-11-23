/* vi: set sw=4 ts=4: */
/*
 * rmdir implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config RMDIR
//config:	bool "rmdir"
//config:	default y
//config:	help
//config:	  rmdir is used to remove empty directories.
//config:
//config:config FEATURE_RMDIR_LONG_OPTIONS
//config:	bool "Enable long options"
//config:	default y
//config:	depends on RMDIR && LONG_OPTS
//config:	help
//config:	  Support long options for the rmdir applet, including
//config:	  --ignore-fail-on-non-empty for compatibility with GNU rmdir.

//applet:IF_RMDIR(APPLET_NOFORK(rmdir, rmdir, BB_DIR_BIN, BB_SUID_DROP, rmdir))

//kbuild:lib-$(CONFIG_RMDIR) += rmdir.o

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/rmdir.html */

//usage:#define rmdir_trivial_usage
//usage:       "[OPTIONS] DIRECTORY..."
//usage:#define rmdir_full_usage "\n\n"
//usage:       "Remove DIRECTORY if it is empty\n"
//usage:	IF_FEATURE_RMDIR_LONG_OPTIONS(
//usage:     "\n	-p|--parents	Include parents"
//usage:     "\n	--ignore-fail-on-non-empty"
//usage:	)
//usage:	IF_NOT_FEATURE_RMDIR_LONG_OPTIONS(
//usage:     "\n	-p	Include parents"
//usage:	)
//usage:
//usage:#define rmdir_example_usage
//usage:       "# rmdir /tmp/foo\n"

#include "libbb.h"

/* This is a NOFORK applet. Be very careful! */


#define PARENTS          (1 << 0)
#define VERBOSE          ((1 << 1) * ENABLE_FEATURE_VERBOSE)
#define IGNORE_NON_EMPTY (1 << 2)

int rmdir_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int rmdir_main(int argc UNUSED_PARAM, char **argv)
{
	int status = EXIT_SUCCESS;
	int flags;
	char *path;

#if ENABLE_FEATURE_RMDIR_LONG_OPTIONS
	static const char rmdir_longopts[] ALIGN1 =
		"parents\0"                  No_argument "p"
		/* Debian etch: many packages fail to be purged or installed
		 * because they desperately want this option: */
		"ignore-fail-on-non-empty\0" No_argument "\xff"
		IF_FEATURE_VERBOSE(
		"verbose\0"                  No_argument "v"
		)
		;
	applet_long_options = rmdir_longopts;
#endif
	flags = getopt32(argv, "pv");
	argv += optind;

	if (!*argv) {
		bb_show_usage();
	}

	do {
		path = *argv;

		while (1) {
			if (flags & VERBOSE) {
				printf("rmdir: removing directory, '%s'\n", path);
			}

			if (rmdir(path) < 0) {
#if ENABLE_FEATURE_RMDIR_LONG_OPTIONS
				if ((flags & IGNORE_NON_EMPTY) && errno == ENOTEMPTY)
					break;
#endif
				bb_perror_msg("'%s'", path);  /* Match gnu rmdir msg. */
				status = EXIT_FAILURE;
			} else if (flags & PARENTS) {
				/* Note: path was not "" since rmdir succeeded. */
				path = dirname(path);
				/* Path is now just the parent component.  Dirname
				 * returns "." if there are no parents.
				 */
				if (NOT_LONE_CHAR(path, '.')) {
					continue;
				}
			}
			break;
		}
	} while (*++argv);

	return status;
}
