/* vi: set sw=4 ts=4: */
/*
 * Mini sort implementation for busybox
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

/* BB_AUDIT SUSv3 _NOT_ compliant -- a number of options are not supported. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/sort.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Now does proper error checking on i/o.  Plus some space savings.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "busybox.h"
#include "libcoreutils/coreutils.h"

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
	int i, nlines = 0, inc;
	int (*compare)(const void *, const void *) = compare_ascii;

	int flags;

	bb_default_error_retval = 2;

	flags = bb_getopt_ulflags(argc, argv, "nru");
	if (flags & 1) {
		compare = compare_numeric;
	}

	argv += optind;
	if (!*argv) {
		*--argv = "-";
	}

	do {
		fp = xgetoptfile_sort_uniq(argv, "r");
		while ((line = bb_get_chomped_line_from_file(fp)) != NULL) {
			lines = xrealloc(lines, sizeof(char *) * (nlines + 1));
			lines[nlines++] = line;
		}
		bb_xferror(fp, *argv);
		bb_fclose_nonstdin(fp);
	} while (*++argv);

	/* sort it */
	qsort(lines, nlines, sizeof(char *), compare);

	/* print it */
	i = 0;
	--nlines;
	if ((inc = 1 - (flags & 2)) < 0) {	/* reverse */
		i = nlines;
	}
	flags &= 4;

	while (nlines >= 0) {
		if (!flags || !nlines || strcmp(lines[i+inc], lines[i])) {
			puts(lines[i]);
		}
		i += inc;
		--nlines;
	}

	bb_fflush_stdout_and_exit(EXIT_SUCCESS);
}
