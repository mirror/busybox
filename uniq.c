/* vi: set sw=4 ts=4: */
/*
 * Mini uniq implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
 * Written by John Beppu <beppu@codepoet.org>
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

#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <stdlib.h>
#include "busybox.h"

static int print_count;
static int print_uniq = 1;
static int print_duplicates = 1;

static void print_line(char *line, int count, FILE *fp)
{
	if ((print_duplicates && count > 1) || (print_uniq && count == 1)) {
		if (print_count)
			fprintf(fp, "%7d\t%s", count, line);
		else
			fputs(line, fp);
	}
}

int uniq_main(int argc, char **argv)
{
	FILE *in = stdin, *out = stdout;
	char *lastline = NULL, *input;
	int opt, count = 0;

	/* parse argv[] */
	while ((opt = getopt(argc, argv, "cdu")) > 0) {
		switch (opt) {
			case 'c':
				print_count = 1;
				break;
			case 'd':
				print_duplicates = 1;
				print_uniq = 0;
				break;
			case 'u':
				print_duplicates = 0;
				print_uniq = 1;
				break;
		}
	}

	if (argv[optind] != NULL) {
		in = xfopen(argv[optind], "r");
		if (argv[optind+1] != NULL)
			out = xfopen(argv[optind+1], "w");
	}

	while ((input = get_line_from_file(in)) != NULL) {
		if (lastline == NULL || strcmp(input, lastline) != 0) {
			print_line(lastline, count, out);
			free(lastline);
			lastline = input;
			count = 0;
		}
		count++;
	}
	print_line(lastline, count, out);
	free(lastline);

	return EXIT_SUCCESS;
}
