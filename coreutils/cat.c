/*
 * Mini Cat implementation for busybox
 *
 * Copyright (C) 1998 by Erik Andersen <andersee@debian.org>
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

#include "internal.h"
#include <stdio.h>

const char cat_usage[] = "[file ...]";

extern int cat_more_main(int argc, char **argv)
{
    int c;
    FILE *file = stdin;

    if (argc < 2) {
	fprintf(stderr, "Usage: %s %s", *argv, cat_usage);
	return(FALSE);
    }
    argc--;
    argv++;

    while (argc-- > 0) {
	file = fopen(*argv, "r");
	if (file == NULL) {
	    perror(*argv);
	    return(FALSE);
	}
	while ((c = getc(file)) != EOF)
	    putc(c, stdout);
	fclose(file);
	fflush(stdout);

	argc--;
	argv++;
    }
    return(TRUE);
}
