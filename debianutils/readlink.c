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
	unsigned long opt = ENABLE_FEATURE_READLINK_FOLLOW ?
							bb_getopt_ulflags(argc, argv, "f") : 0;

	if (argc != (ENABLE_FEATURE_READLINK_FOLLOW ? optind + 1 : 2))
			bb_show_usage();

	if (opt & READLINK_FLAG_f)
		buf = realpath(argv[optind], bb_common_bufsiz1);
	else
		buf = xreadlink(argv[ENABLE_FEATURE_READLINK_FOLLOW ? optind : 1]);

	if (!buf)
		return EXIT_FAILURE;
	puts(buf);

	if (ENABLE_FEATURE_CLEAN_UP && buf != bb_common_bufsiz1) free(buf);

	return EXIT_SUCCESS;
}
