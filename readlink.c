/* vi: set sw=4 ts=4: */
/*
 * Mini readlink implementation for busybox
 *
 * Copyright (C) 1999,2000,2001 by Matt Kraai <kraai@alumni.carnegiemellon.edu>
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
#include "busybox.h"

int readlink_main(int argc, char **argv)
{
	char *buf = NULL;

	/* no options, no getopt */

	if (argc != 2)
		show_usage();

	buf = xreadlink(argv[1]);
	if (!buf)
		return EXIT_FAILURE;
	puts(buf);
#ifdef BB_FEATURE_CLEAN_UP
	free(buf);
#endif

	return EXIT_SUCCESS;
}
