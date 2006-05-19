/* vi: set sw=4 ts=4: */
/*
 * Mini chgrp implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 defects - unsupported options -h, -H, -L, and -P. */
/* BB_AUDIT GNU defects - unsupported options -h, -c, -f, -v, and long options. */
/* BB_AUDIT Note: gnu chgrp does not support -H, -L, or -P. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/chgrp.html */

#include <stdlib.h>
#include <unistd.h>
#include "busybox.h"

/* Don't use lchown glibc older then 2.1.x */
#if (__GLIBC__ <= 2) && (__GLIBC_MINOR__ < 1)
#define lchown	chown
#endif

static int fileAction(const char *fileName, struct stat *statbuf, void* junk)
{
	if (lchown(fileName, statbuf->st_uid, *((long *) junk)) == 0) {
		return (TRUE);
	}
	bb_perror_msg("%s", fileName);	/* Avoid multibyte problems. */
	return (FALSE);
}

int chgrp_main(int argc, char **argv)
{
	long gid;
	int recursiveFlag;
	int retval = EXIT_SUCCESS;

	recursiveFlag = bb_getopt_ulflags(argc, argv, "R");

	if (argc - optind < 2) {
		bb_show_usage();
	}

	argv += optind;

	/* Find the selected group */
	gid = get_ug_id(*argv, bb_xgetgrnam);
	++argv;

	/* Ok, ready to do the deed now */
	do {
		if (! recursive_action (*argv, recursiveFlag, FALSE, FALSE,
								fileAction, fileAction, &gid)) {
			retval = EXIT_FAILURE;
		}
	} while (*++argv);

	return retval;
}
