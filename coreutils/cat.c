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

#include "libbb.h"

/* This is a NOFORK applet. Be very careful! */


int bb_cat(char **argv)
{
	static const char *const argv_dash[] = { "-", NULL };

	int fd;
	int retval = EXIT_SUCCESS;

	if (!*argv)
		argv = (char**) &argv_dash;

	do {
		fd = STDIN_FILENO;
		if (!LONE_DASH(*argv))
			fd = open_or_warn(*argv, O_RDONLY);
		if (fd >= 0) {
			/* This is not an xfunc - never exits */
			off_t r = bb_copyfd_eof(fd, STDOUT_FILENO);
			if (fd != STDIN_FILENO)
				close(fd);
			if (r >= 0)
				continue;
		}
		retval = EXIT_FAILURE;
	} while (*++argv);

	return retval;
}

int cat_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int cat_main(int argc, char **argv)
{
	getopt32(argv, "u");
	argv += optind;
	return bb_cat(argv);
}
