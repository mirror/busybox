/* vi: set sw=4 ts=4: */
/*
 * usleep implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 N/A -- Apparently a busybox extension. */

#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include "busybox.h"

int usleep_main(int argc, char **argv);
int usleep_main(int argc, char **argv)
{
	if (argc != 2) {
		bb_show_usage();
	}

	if (usleep(xatou(argv[1]))) {
		bb_perror_nomsg_and_die();
	}

	return EXIT_SUCCESS;
}
