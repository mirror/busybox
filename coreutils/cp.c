/* vi: set sw=4 ts=4: */
/*
 * Mini cp implementation for busybox
 *
 * Copyright (C) 2000 by Matt Kraai <kraai@alumni.carnegiemellon.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
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

/* WARNING!! ORDER IS IMPORTANT!! */
static const char cp_opts[] = "pdRfiarP";

extern int cp_main(int argc, char **argv)
{
	struct stat source_stat;
	struct stat dest_stat;
	const char *last;
	const char *dest;
	int s_flags;
	int d_flags;
	int flags;
	int status = 0;

	/* Since these are enums, #if tests will not work.  So use assert()s. */
	assert(FILEUTILS_PRESERVE_STATUS == 1);
	assert(FILEUTILS_DEREFERENCE == 2);
	assert(FILEUTILS_RECUR == 4);
	assert(FILEUTILS_FORCE == 8);
	assert(FILEUTILS_INTERACTIVE == 16);

	flags = bb_getopt_ulflags(argc, argv, cp_opts);

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
