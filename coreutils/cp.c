/* vi: set sw=4 ts=4: */
/*
 * Mini cp implementation for busybox
 *
 * Copyright (C) 2000 by Matt Kraai <kraai@alumni.carnegiemellon.edu>
 *
 * Licensed under GPL v2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 defects - unsupported options -H, -L, and -P. */
/* BB_AUDIT GNU defects - only extension options supported are -a and -d.  */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/cp.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Size reduction.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <utime.h>
#include <errno.h>
#include <dirent.h>
#include <stdlib.h>
#include <assert.h>
#include "busybox.h"
#include "libcoreutils/coreutils.h"

int cp_main(int argc, char **argv)
{
	struct stat source_stat;
	struct stat dest_stat;
	const char *last;
	const char *dest;
	int s_flags;
	int d_flags;
	int flags;
	int status = 0;

	flags = bb_getopt_ulflags(argc, argv, "pdRfiarPHL");

	if (flags & 32) {
		flags |= (FILEUTILS_PRESERVE_STATUS | FILEUTILS_RECUR | FILEUTILS_DEREFERENCE);
	}
	if (flags & 64) {
		/* Make -r a synonym for -R,
		 * -r was marked as obsolete in SUSv3, but is included for compatability
		 */
		flags |= FILEUTILS_RECUR;
	}
	if (flags & 128) {
		/* Make -P a synonym for -d,
		 * -d is the GNU option while -P is the POSIX 2003 option
		 */
		flags |= FILEUTILS_DEREFERENCE;
	}
	/* Default behavior of cp is to dereference, so we don't have to do
	 * anything special when we are given -L.
	 * The behavior of -H is *almost* like -L, but not quite, so let's
	 * just ignore it too for fun.
	if (flags & 256 || flags & 512) {
		;
	}
	*/

	flags ^= FILEUTILS_DEREFERENCE;		/* The sense of this flag was reversed. */

	if (optind + 2 > argc) {
		bb_show_usage();
	}

	last = argv[argc - 1];
	argv += optind;

	/* If there are only two arguments and...  */
	if (optind + 2 == argc) {
		s_flags = cp_mv_stat2(*argv, &source_stat,
		                      (flags & FILEUTILS_DEREFERENCE) ? stat : lstat);
		if ((s_flags < 0) || ((d_flags = cp_mv_stat(last, &dest_stat)) < 0)) {
			exit(EXIT_FAILURE);
		}
		/* ...if neither is a directory or...  */
		if ( !((s_flags | d_flags) & 2) ||
			/* ...recursing, the 1st is a directory, and the 2nd doesn't exist... */
			/* ((flags & FILEUTILS_RECUR) && (s_flags & 2) && !d_flags) */
			/* Simplify the above since FILEUTILS_RECUR >> 1 == 2. */
			((((flags & FILEUTILS_RECUR) >> 1) & s_flags) && !d_flags)
		) {
			/* ...do a simple copy.  */
			dest = last;
			goto DO_COPY; /* Note: optind+2==argc implies argv[1]==last below. */
		}
	}

	do {
		dest = concat_path_file(last, bb_get_last_path_component(*argv));
	DO_COPY:
		if (copy_file(*argv, dest, flags) < 0) {
			status = 1;
		}
		if (*++argv == last) {
			break;
		}
		free((void *) dest);
	} while (1);

	exit(status);
}
