/*
 * Mini Cat implementation for busybox
 *
 * Copyright (C) 1999 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
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


static void print_file( FILE *file) 
{
    int c;
    while ((c = getc(file)) != EOF)
	putc(c, stdout);
    fclose(file);
    fflush(stdout);
}

extern int cat_main(int argc, char **argv)
{
    FILE *file;

    if (argc==1) {
	print_file( stdin);
	exit( TRUE);
    }

    if ( **(argv+1) == '-' ) {
	usage ("cat [file ...]\n");
    }
    argc--;
    argv++;

    while (argc-- > 0) {
	file = fopen(*argv, "r");
	if (file == NULL) {
	    perror(*argv);
	    exit(FALSE);
	}
	print_file( file);
	argc--;
	argv++;
    }
    exit(TRUE);
}
