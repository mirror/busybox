/* vi: set sw=4 ts=4: */

/* BB_AUDIT SUSv3 N/A -- Apparently a busybox extension. */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Now does proper error checking on output and returns a failure exit code
 * if one or more paths cannot be resolved.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"

int realpath_main(int argc, char **argv)
{
	int retval = EXIT_SUCCESS;

#if PATH_MAX > (BUFSIZ+1)
	RESERVE_CONFIG_BUFFER(resolved_path, PATH_MAX);
# define resolved_path_MUST_FREE 1
#else
#define resolved_path bb_common_bufsiz1
# define resolved_path_MUST_FREE 0
#endif

	if (--argc == 0) {
		bb_show_usage();
	}

	do {
		argv++;
		if (realpath(*argv, resolved_path) != NULL) {
			puts(resolved_path);
		} else {
			retval = EXIT_FAILURE;
			bb_perror_msg("%s", *argv);
		}
	} while (--argc);

#if ENABLE_FEATURE_CLEAN_UP && resolved_path_MUST_FREE
	RELEASE_CONFIG_BUFFER(resolved_path);
#endif

	fflush_stdout_and_exit(retval);
}
