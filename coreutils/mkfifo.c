/* vi: set sw=4 ts=4: */
/*
 * mkfifo implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/mkfifo.html */

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include "busybox.h"
#include "libcoreutils/coreutils.h"

int mkfifo_main(int argc, char **argv);
int mkfifo_main(int argc, char **argv)
{
	mode_t mode;
	int retval = EXIT_SUCCESS;

	mode = getopt_mk_fifo_nod(argc, argv);

	if (!*(argv += optind)) {
		bb_show_usage();
	}

	do {
		if (mkfifo(*argv, mode) < 0) {
			bb_perror_msg("%s", *argv);	/* Avoid multibyte problems. */
			retval = EXIT_FAILURE;
		}
	} while (*++argv);

	return retval;
}
