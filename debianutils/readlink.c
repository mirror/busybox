/* vi: set sw=4 ts=4: */
/*
 * Mini readlink implementation for busybox
 *
 * Copyright (C) 2000,2001 Matt Kraai <kraai@alumni.carnegiemellon.edu>
 *
 * Licensed under GPL v2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>

int readlink_main(int argc, char **argv);
int readlink_main(int argc, char **argv)
{
	char *buf;
	char *fname;

	USE_FEATURE_READLINK_FOLLOW(
		unsigned opt;
		/* We need exactly one non-option argument.  */
		opt_complementary = "=1";
		opt = getopt32(argc, argv, "f");
		fname = argv[optind];
	)
	SKIP_FEATURE_READLINK_FOLLOW(
		const unsigned opt = 0;
		if (argc != 2) bb_show_usage();
		fname = argv[1];
	)

	/* compat: coreutils readlink reports errors silently via exit code */
	logmode = LOGMODE_NONE;

	if (opt) {
		buf = realpath(fname, bb_common_bufsiz1);
	} else {
		buf = xmalloc_readlink_or_warn(fname);
	}

	if (!buf)
		return EXIT_FAILURE;
	puts(buf);

	if (ENABLE_FEATURE_CLEAN_UP && buf != bb_common_bufsiz1)
		free(buf);

	fflush_stdout_and_exit(EXIT_SUCCESS);
}
