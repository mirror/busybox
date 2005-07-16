/* vi: set sw=4 ts=4: */
/*
 * Mini cmp implementation for busybox
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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <getopt.h>
#include "busybox.h"

int cmp_main(int argc, char **argv)
{
	FILE *fp1 = NULL, *fp2 = stdin;
	char *filename1, *filename2 = "-";
	int c, c1, c2, char_pos = 1, line_pos = 1, silent = FALSE;

	while ((c = getopt(argc, argv, "s")) != EOF) {
		switch (c) {
			case 's':
				silent = TRUE;
				break;
			default:
				show_usage();
		}
	}

	filename1 = argv[optind];
	switch (argc - optind) {
		case 2:
			fp2 = xfopen(filename2 = argv[optind + 1], "r");
		case 1:
			fp1 = xfopen(filename1, "r");
			break;
		default:
			show_usage();
	}

	do {
		c1 = fgetc(fp1);
		c2 = fgetc(fp2);
		if (c1 != c2) {
			if (silent)
				return EXIT_FAILURE;
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
