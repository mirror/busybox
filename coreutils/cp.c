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

#include "libbb.h"
#include "libcoreutils/coreutils.h"

/* This is a NOEXEC applet. Be very careful! */


int cp_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
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
	};

	// Need at least two arguments
	// Soft- and hardlinking doesn't mix
	// -P and -d are the same (-P is POSIX, -d is GNU)
	// -r and -R are the same
	// -R (and therefore -r) turns on -d (coreutils does this)
	// -a = -pdR
	opt_complementary = "-2:l--s:s--l:Pd:rRd:Rd:apdR:HL";
	flags = getopt32(argv, FILEUTILS_CP_OPTSTR "arPH");
	argc -= optind;
	argv += optind;
	flags ^= FILEUTILS_DEREFERENCE; /* the sense of this flag was reversed */
	/* coreutils 6.9 compat:
	 * by default, "cp" derefs symlinks (creates regular dest files),
	 * but "cp -R" does not. We switch off deref if -r or -R (see above).
	 * However, "cp -RL" must still deref symlinks: */
	if (flags & FILEUTILS_DEREF_SOFTLINK) /* -L */
		flags |= FILEUTILS_DEREFERENCE;
	/* The behavior of -H is *almost* like -L, but not quite, so let's
	 * just ignore it too for fun. TODO.
	if (flags & OPT_H) ... // deref command-line params only
	*/

#if ENABLE_SELINUX
	if (flags & FILEUTILS_PRESERVE_SECURITY_CONTEXT) {
		selinux_or_die();
	}
#endif

	last = argv[argc - 1];
	/* If there are only two arguments and...  */
	if (argc == 2) {
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
			dest = last;
			goto DO_COPY; /* NB: argc==2 -> *++argv==last */
		}
	}

	while (1) {
		dest = concat_path_file(last, bb_get_last_path_component_strip(*argv));
 DO_COPY:
		if (copy_file(*argv, dest, flags) < 0) {
			status = 1;
		}
		if (*++argv == last) {
			/* possibly leaking dest... */
			break;
		}
		free((void*)dest);
	}

	/* Exit. We are NOEXEC, not NOFORK. We do exit at the end of main() */
	return status;
}
