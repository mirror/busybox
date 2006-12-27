/* vi: set sw=4 ts=4: */
/*
 * Mini touch implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 _NOT_ compliant -- options -a, -m, -r, -t not supported. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/touch.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Previous version called open() and then utime().  While this will be
 * be necessary to implement -r and -t, it currently only makes things bigger.
 * Also, exiting on a failure was a bug.  All args should be processed.
 */

#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <utime.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include "busybox.h"

int touch_main(int argc, char **argv)
{
	int fd;
	int flags;
	int status = EXIT_SUCCESS;

	flags = getopt32(argc, argv, "c");

	argv += optind;

	if (!*argv) {
		bb_show_usage();
	}

	do {
		if (utime(*argv, NULL)) {
			if (errno == ENOENT) {	/* no such file*/
				if (flags & 1) {	/* Creation is disabled, so ignore. */
					continue;
				}
				/* Try to create the file. */
				fd = open(*argv, O_RDWR | O_CREAT,
						  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
						  );
				if ((fd >= 0) && !close(fd)) {
					continue;
				}
			}
			status = EXIT_FAILURE;
			bb_perror_msg("%s", *argv);
		}
	} while (*++argv);

	return status;
}
