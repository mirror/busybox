/* vi: set sw=4 ts=4: */
/*
 * cat implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2, see file License in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/cat.html */

#include "busybox.h"

int bb_cat(char **argv)
{
	static const char *const argv_dash[] = { "-", NULL };
	FILE *f;
	int retval = EXIT_SUCCESS;

	if (!*argv) argv = (char**) &argv_dash;

	do {
		f = fopen_or_warn_stdin(*argv);
		if (f) {
			off_t r = bb_copyfd_eof(fileno(f), STDOUT_FILENO);
			fclose_if_not_stdin(f);
			if (r >= 0)
				continue;
		}
		retval = EXIT_FAILURE;
	} while (*++argv);

	return retval;
}

int cat_main(int argc, char **argv);
int cat_main(int argc, char **argv)
{
	getopt32(argc, argv, "u");
	argv += optind;
	return bb_cat(argv);
}
