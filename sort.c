/* vi: set sw=4 ts=4: */
/*
 * Mini sort implementation for busybox
 *
 *
 * Copyright (C) 2000 by Matt Kraai <kraai@alumni.carnegiemellon.edu>
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
#include <string.h>
#include <stdlib.h>
#include "busybox.h"

static int compare_ascii(const void *x, const void *y)
{
	return strcmp(*(char **)x, *(char **)y);
}

static int compare_numeric(const void *x, const void *y)
{
	int z = atoi(*(char **)x) - atoi(*(char **)y);
	return z ? z : strcmp(*(char **)x, *(char **)y);
}

int sort_main(int argc, char **argv)
{
	FILE *fp;
	char *line, **lines = NULL;
	int i, opt, nlines = 0;
	int (*compare)(const void *, const void *) = compare_ascii;
#ifdef BB_FEATURE_SORT_REVERSE
	int reverse = FALSE;
#endif
#ifdef BB_FEATURE_SORT_UNIQUE
	int unique = FALSE;
#endif

	while ((opt = getopt(argc, argv, "nru")) != -1) {
		switch (opt) {
			case 'n':
				compare = compare_numeric;
				break;
#ifdef BB_FEATURE_SORT_REVERSE
			case 'r':
				reverse = TRUE;
				break;
#endif
#ifdef BB_FEATURE_SORT_UNIQUE
			case 'u':
				unique = TRUE;
				break;
#endif
			default:
				show_usage();
		}
	}

	/* read the input */
	for (i = optind; i == optind || i < argc; i++) {
		if (argv[i] == NULL)
			fp = stdin;
		else
			fp = xfopen(argv[i], "r");

		while ((line = get_line_from_file(fp)) != NULL) {
			lines = xrealloc(lines, sizeof(char *) * (nlines + 1));
			chomp(line);
			lines[nlines++] = line;
		}
	}

	/* sort it */
	qsort(lines, nlines, sizeof(char *), compare);

	/* print it */
#ifdef BB_FEATURE_SORT_REVERSE
	if (reverse) {
		for (i = --nlines; 0 <= i; i--)
#ifdef BB_FEATURE_SORT_UNIQUE
			if((!unique) || (i == nlines) || (strcmp(lines[i + 1], lines[i])))
#endif
				puts(lines[i]);
	} else
#endif
		for (i = 0; i < nlines; i++)
#ifdef BB_FEATURE_SORT_UNIQUE
			if((!unique) || (!i) || (strcmp(lines[i - 1], lines[i])))
#endif
				puts(lines[i]);
	return EXIT_SUCCESS;
}
