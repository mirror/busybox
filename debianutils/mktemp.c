/* vi: set sw=4 ts=4: */
/*
 * Mini mktemp implementation for busybox
 *
 *
 * Copyright (C) 2000 by Daniel Jacobowitz
 * Written by Daniel Jacobowitz <dan@debian.org>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "libbb.h"

int mktemp_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int mktemp_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	// -d      Make a directory instead of a file
	// -q      Fail silently if an error occurs [bbox: ignored]
	// -t      Generate a path rooted in temporary directory
	// -p DIR  Use DIR as a temporary directory (implies -t)
	const char *path;
	char *chp;
	unsigned flags;

	opt_complementary = "=1"; /* exactly one arg */
	flags = getopt32(argv, "dqtp:", &path);
	chp = argv[optind];

	if (flags & (4|8)) { /* -t and/or -p */
		const char *dir = getenv("TMPDIR");
		if (dir && *dir != '\0')
			path = dir;
		else if (!(flags & 8)) /* No -p */
			path = "/tmp/";
		/* else path comes from -p DIR */
		chp = concat_path_file(path, chp);
	}

	if (flags & 1) { /* -d */
		if (mkdtemp(chp) == NULL)
			return EXIT_FAILURE;
	} else {
		if (mkstemp(chp) < 0)
			return EXIT_FAILURE;
	}

	puts(chp);

	return EXIT_SUCCESS;
}
