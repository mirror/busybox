/* vi: set sw=4 ts=4: */
/*
 * Mini readlink implementation for busybox
 *
 * Copyright (C) 2000,2001 Matt Kraai <kraai@alumni.carnegiemellon.edu>
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

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include "busybox.h"

#ifdef CONFIG_FEATURE_READLINK_FOLLOW
# define READLINK_FOLLOW	"f"
# define READLINK_FLAG_f	(1 << 0)
#else
# define READLINK_FOLLOW	""
#endif

static const char readlink_options[] = READLINK_FOLLOW;

int readlink_main(int argc, char **argv)
{
	char *buf = NULL;
	unsigned long opt = bb_getopt_ulflags(argc, argv, readlink_options);
#ifdef CONFIG_FEATURE_READLINK_FOLLOW
	RESERVE_CONFIG_BUFFER(resolved_path, PATH_MAX);
#endif

	/* no options, no getopt */

	if (optind + 1 != argc)
		bb_show_usage();

#ifdef CONFIG_FEATURE_READLINK_FOLLOW
	if (opt & READLINK_FLAG_f) {
		buf = realpath(argv[optind], resolved_path);
	} else
#endif
		buf = xreadlink(argv[optind]);

	if (!buf)
		return EXIT_FAILURE;
	puts(buf);
#ifdef CONFIG_FEATURE_CLEAN_UP
	free(buf);
#endif

	return EXIT_SUCCESS;
}
