/* vi: set sw=4 ts=4: */
/*
 * readahead implementation for busybox
 *
 * Preloads the given files in RAM, to reduce access time.
 * Does this by calling the readahead(2) system call.
 *
 * Copyright (C) 2006  Michael Opdenacker <michael@free-electrons.com>
 *
 * Licensed under GPLv2 or later, see file License in this tarball for details.
 */

#include "busybox.h"

int readahead_main(int argc, char **argv)
{
	FILE *f;
	struct stat stat_buf;
	int retval = EXIT_SUCCESS;

	if (argc == 1) bb_show_usage();

	while (*++argv) {
		if ((f = bb_wfopen(*argv, "r")) != NULL) {
			int r, fd=fileno(f);
			
			xstat(*argv, &stat_buf);
			r = readahead(fd, 0, fdlength(fd));
			fclose(f);
			if (r >= 0) continue;
		}
		retval = EXIT_FAILURE;
	}

	return retval;
}
