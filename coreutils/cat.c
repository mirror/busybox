/* vi: set sw=4 ts=4: */
/*
 * cat implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file License in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/cat.html */

#include "busybox.h"
#include <unistd.h>

int cat_main(int argc, char **argv)
{
	FILE *f;
	int retval = EXIT_SUCCESS;

	bb_getopt_ulflags(argc, argv, "u");

	argv += optind;
	if (!*argv) {
		*--argv = "-";
	}

	do {
		if ((f = bb_wfopen_input(*argv)) != NULL) {
			int r = bb_copyfd_eof(fileno(f), STDOUT_FILENO);
			bb_fclose_nonstdin(f);
			if (r >= 0) {
				continue;
			}
		}
		retval = EXIT_FAILURE;
	} while (*++argv);

	return retval;
}
