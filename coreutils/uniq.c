/* vi: set sw=4 ts=4: */
/*
 * Mini uniq implementation for busybox
 *
 *
 * Copyright (C) 1999,2000 by Lineo, inc.
 * Written by John Beppu <beppu@lineo.com>
 * Rewritten by Matt Kraai <kraai@alumni.carnegiemellon.edu>
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

#include "busybox.h"
#include <stdio.h>
#include <string.h>
#include <errno.h>

int uniq_main(int argc, char **argv)
{
	FILE *in = stdin, *out = stdout;
	char *lastline = NULL, *input;

	/* parse argv[] */
	if ((argc > 1 && **(argv + 1) == '-') || argc > 3)
		usage(uniq_usage);

	if (argv[1] != NULL) {
		in = xfopen(argv[1], "r");
		if (argv[2] != NULL)
			out = xfopen(argv[2], "w");
	}

	while ((input = get_line_from_file(in)) != NULL) {
		if (lastline == NULL || strcmp(input, lastline) != 0) {
			fputs(input, out);
			free(lastline);
			lastline = input;
		}
	}
	free(lastline);

	return EXIT_SUCCESS;
}
