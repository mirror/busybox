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

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "busybox.h"

int mktemp_main(int argc, char **argv)
{
	unsigned long flags = bb_getopt_ulflags(argc, argv, "dq");

	if (optind + 1 != argc)
		bb_show_usage();

	if (flags & 1) {
		if (mkdtemp(argv[optind]) == NULL)
			return EXIT_FAILURE;
	}
	else {
		if (mkstemp(argv[optind]) < 0)
			return EXIT_FAILURE;
	}

	puts(argv[optind]);

	return EXIT_SUCCESS;
}
