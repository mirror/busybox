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

#include "busybox.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

int mktemp_main(int argc, char **argv);
int mktemp_main(int argc, char **argv)
{
	unsigned long flags = getopt32(argc, argv, "dqt");
	char *chp;

	if (optind + 1 != argc)
		bb_show_usage();

	chp = argv[optind];

	if (flags & 4) {
		char *dir = getenv("TMPDIR");
		if (dir && *dir != '\0')
			chp = concat_path_file(dir, chp);
		else
			chp = concat_path_file("/tmp/", chp);
	}

	if (flags & 1) {
		if (mkdtemp(chp) == NULL)
			return EXIT_FAILURE;
	} else {
		if (mkstemp(chp) < 0)
			return EXIT_FAILURE;
	}

	puts(chp);

	return EXIT_SUCCESS;
}
