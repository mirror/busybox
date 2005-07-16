/* vi: set sw=4 ts=4: */
/*
 * Mini rmdir implementation for busybox
 *
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
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

#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>

#include "busybox.h"


/* Return true if a path is composed of multiple components.  */

static int
multiple_components_p (const char *path)
{
	const char *s = path;

	while (s[0] != '\0' && s[0] != '/')
		s++;

	while (s[0] == '/')
		s++;

	return (s[0] != '\0');
}


/* Remove a directory.  Returns 0 if successful, -1 on error.  */

static int
remove_directory (char *path, int flags)
{
	if (!(flags & FILEUTILS_RECUR)) {
		if (rmdir (path) < 0) {
			perror_msg ("unable to remove `%s'", path);
			return -1;
		}
	} else {
		if (remove_directory (path, 0) < 0)
			return -1;

		if (multiple_components_p (path))
			if (remove_directory (dirname (path), flags) < 0)
				return -1;
	}

	return 0;
}


extern int
rmdir_main (int argc, char **argv)
{
	int status = EXIT_SUCCESS;
	int flags = 0;
	int i, opt;

	while ((opt = getopt (argc, argv, "p")) != -1)
		switch (opt) {
			case 'p':
				flags |= FILEUTILS_RECUR;
				break;

			default:
				show_usage ();
		}

	if (optind == argc)
		show_usage();

	for (i = optind; i < argc; i++)
		if (remove_directory (argv[i], flags) < 0)
			status = EXIT_FAILURE;

	return status;
}
