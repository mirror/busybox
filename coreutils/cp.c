/* vi: set sw=4 ts=4: */
/*
 * Mini cp implementation for busybox
 *
 * Copyright (C) 2000 by Matt Kraai <kraai@alumni.carnegiemellon.edu>
 * SELinux support by Yuichi Nakamura <ynakam@hitachisoft.jp>
 *
 * Licensed under GPL v2 or later, see file LICENSE in this tarball for details.
 */

/* http://www.opengroup.org/onlinepubs/007904975/utilities/cp.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Size reduction.
 */

#include "busybox.h"
#include "libcoreutils/coreutils.h"

int cp_main(int argc, char **argv);
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
	enum {
		OPT_a = 1 << (sizeof(FILEUTILS_CP_OPTSTR)-1),
		OPT_r = 1 << (sizeof(FILEUTILS_CP_OPTSTR)),
		OPT_P = 1 << (sizeof(FILEUTILS_CP_OPTSTR)+1),
		OPT_H = 1 << (sizeof(FILEUTILS_CP_OPTSTR)+2),
		OPT_L = 1 << (sizeof(FILEUTILS_CP_OPTSTR)+3),
	};

	// Soft- and hardlinking don't mix
	// -P and -d are the same (-P is POSIX, -d is GNU)
	// -r and -R are the same
	// -a = -pdR
	opt_complementary = "?:l--s:s--l:Pd:rR:apdR";
	flags = getopt32(argc, argv, FILEUTILS_CP_OPTSTR "arPHL");
	/* Default behavior of cp is to dereference, so we don't have to do
	 * anything special when we are given -L.
	 * The behavior of -H is *almost* like -L, but not quite, so let's
	 * just ignore it too for fun.
	if (flags & OPT_L) ...
	if (flags & OPT_H) ... // deref command-line params only
	*/

#if ENABLE_SELINUX
	if (flags & FILEUTILS_PRESERVE_SECURITY_CONTEXT) {
		selinux_or_die();
	}
#endif

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
		if (s_flags < 0)
			return EXIT_FAILURE;
		d_flags = cp_mv_stat(last, &dest_stat);
		if (d_flags < 0)
			return EXIT_FAILURE;

		/* ...if neither is a directory or...  */
		if ( !((s_flags | d_flags) & 2) ||
			/* ...recursing, the 1st is a directory, and the 2nd doesn't exist... */
			((flags & FILEUTILS_RECUR) && (s_flags & 2) && !d_flags)
		) {
			/* ...do a simple copy.  */
			dest = xstrdup(last);
			goto DO_COPY; /* Note: optind+2==argc implies argv[1]==last below. */
		}
	}

	do {
		dest = concat_path_file(last, bb_get_last_path_component(*argv));
 DO_COPY:
		if (copy_file(*argv, dest, flags) < 0) {
			status = 1;
		}
		free((void*)dest);
	} while (*++argv != last);

	return status;
}
