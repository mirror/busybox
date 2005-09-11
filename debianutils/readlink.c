/* vi: set sw=4 ts=4: */
/*
 * Mini readlink implementation for busybox
 *
 * Copyright (C) 2000,2001 Matt Kraai <kraai@alumni.carnegiemellon.edu>
 *
 * Licensed under GPL v2, see file LICENSE in this tarball for details.
 */

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include "busybox.h"

#define READLINK_FLAG_f	(1 << 0)

int readlink_main(int argc, char **argv)
{
	char *buf;
	unsigned long opt = bb_getopt_ulflags(argc, argv,
							ENABLE_FEATURE_READLINK_FOLLOW ? "f" : "");

	if (optind + 1 != argc)
		bb_show_usage();

	if (ENABLE_FEATURE_READLINK_FOLLOW && (opt & READLINK_FLAG_f))
		buf = realpath(argv[optind], NULL);
	else
		buf = xreadlink(argv[optind]);

	if (!buf)
		return EXIT_FAILURE;
	puts(buf);

	if (ENABLE_FEATURE_CLEAN_UP) free(buf);

	return EXIT_SUCCESS;
}
