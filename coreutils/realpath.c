/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* BB_AUDIT SUSv3 N/A -- Apparently a busybox extension. */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Now does proper error checking on output and returns a failure exit code
 * if one or more paths can not be resolved.
 */

#include <limits.h>
#include <stdlib.h>
#include "busybox.h"

int realpath_main(int argc, char **argv)
{
	int retval = EXIT_SUCCESS;

	RESERVE_CONFIG_BUFFER(resolved_path, PATH_MAX);

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

#ifdef CONFIG_FEATURE_CLEAN_UP
	RELEASE_CONFIG_BUFFER(resolved_path);
#endif

	bb_fflush_stdout_and_exit(retval);
}
