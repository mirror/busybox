/* vi: set sw=4 ts=4: */
/*
 * Mini cmp implementation for busybox
 *
 *
 * Copyright (C) 2000 by Lineo, inc.
 * Written by Matt Kraai <kraai@alumni.carnegiemellon.edu>
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

int cmp_main(int argc, char **argv)
{
	FILE *fp1 = NULL, *fp2 = stdin;
	char *filename1 = argv[1], *filename2 = "-";
	int c1, c2, char_pos = 1, line_pos = 1;

	/* parse argv[] */
	if (argc < 2 || 3 < argc)
		usage(cmp_usage);

	fp1 = xfopen(argv[1], "r");
	if (argv[2] != NULL) {
		fp2 = xfopen(argv[2], "r");
		filename2 = argv[2];
	}

	do {
		c1 = fgetc(fp1);
		c2 = fgetc(fp2);
		if (c1 != c2) {
			if (c1 == EOF)
				printf("EOF on %s\n", filename1);
			else if (c2 == EOF)
				printf("EOF on %s\n", filename2);
			else
				printf("%s %s differ: char %d, line %d\n", filename1, filename2,
						char_pos, line_pos);
			return EXIT_FAILURE;
		}
		char_pos++;
		if (c1 == '\n')
			line_pos++;
	} while (c1 != EOF);

	return EXIT_SUCCESS;
}
