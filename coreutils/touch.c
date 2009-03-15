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

#include "libbb.h"

/* This is a NOFORK applet. Be very careful! */

/* coreutils implements:
 * -a   change only the access time
 * -c, --no-create
 *      do not create any files
 * -d, --date=STRING
 *      parse STRING and use it instead of current time
 * -f   (ignored, BSD compat)
 * -m   change only the modification time
 * -r, --reference=FILE
 *      use this file's times instead of current time
 * -t STAMP
 *      use [[CC]YY]MMDDhhmm[.ss] instead of current time
 * --time=WORD
 *      change the specified time: WORD is access, atime, or use
 */

int touch_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int touch_main(int argc UNUSED_PARAM, char **argv)
{
#if ENABLE_DESKTOP
#if ENABLE_GETOPT_LONG
	static const char longopts[] ALIGN1 =
		/* name, has_arg, val */
		"no-create\0"         No_argument       "c"
		"reference\0"         Required_argument "r"
	;
#endif
	struct utimbuf timebuf;
	char *reference_file = NULL;
#else
#define reference_file NULL
#define timebuf        (*(struct utimbuf*)NULL)
#endif
	int fd;
	int status = EXIT_SUCCESS;
	int opts;

#if ENABLE_DESKTOP
#if ENABLE_GETOPT_LONG
	applet_long_options = longopts;
#endif
#endif
	opts = getopt32(argv, "c" USE_DESKTOP("r:")
				/*ignored:*/ "fma"
				USE_DESKTOP(, &reference_file));

	opts &= 1; /* only -c bit is left */
	argv += optind;
	if (!*argv) {
		bb_show_usage();
	}

	if (reference_file) {
		struct stat stbuf;
		xstat(reference_file, &stbuf);
		timebuf.actime = stbuf.st_atime;
		timebuf.modtime = stbuf.st_mtime;
	}

	do {
		if (utime(*argv, reference_file ? &timebuf : NULL)) {
			if (errno == ENOENT) { /* no such file */
				if (opts) { /* creation is disabled, so ignore */
					continue;
				}
				/* Try to create the file. */
				fd = open(*argv, O_RDWR | O_CREAT,
						  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH
						  );
				if ((fd >= 0) && !close(fd)) {
					if (reference_file)
						utime(*argv, &timebuf);
					continue;
				}
			}
			status = EXIT_FAILURE;
			bb_simple_perror_msg(*argv);
		}
	} while (*++argv);

	return status;
}
