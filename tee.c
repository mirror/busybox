/*
 * Mini tee implementation for busybox
 *
 *
 * Copyright (C) 1999 by Lineo, inc.
 * Written by John Beppu <beppu@lineo.com>
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

static const char tee_usage[] =
"Usage: tee [OPTION]... [FILE]...\n"
"Copy standard input to each FILE, and also to standard output.\n\n"
"  -a,    append to the given FILEs, do not overwrite\n"
"  -i,    ignore interrupt signals\n"
"  -h,    this help message\n";

/* FileList _______________________________________________________________ */

#define FL_MAX	1024
static FILE *FileList[FL_MAX];
static int  FL_end;

typedef void (FL_Function)(FILE *file, char c);
    
/* initialize FileList */
static void
FL_init()
{
    FL_end = 0;
    FileList[0] = stdout;
}

/* add a file to FileList */
static int
FL_add(const char *filename, char *opt_open)
{
    FILE    *file;

    file = fopen(filename, opt_open);
    if (!file) { return 0; };
    if (FL_end < FL_MAX) {
	FileList[++FL_end] = file;
    }
    return 1;
}

/* apply a function to everything in FileList */
static void
FL_apply(FL_Function *f, char c)
{
    int i;
    for (i = 0; i <= FL_end; i++) {
	f(FileList[i], c);
    }
}

/* ________________________________________________________________________ */

/* FL_Function for writing to files*/
static void
tee_fwrite(FILE *file, char c)
{
    fputc(c, file);
}

/* FL_Function for closing files */
static void
tee_fclose(FILE *file, char c)
{
    fclose(file);
}

/* BusyBoxed tee(1) */
int
tee_main(int argc, char **argv)
{
    int	    i;
    char    c;
    char    opt;
    char    opt_fopen[2] = "w";

    /* parse argv[] */
    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    opt = argv[i][1];
	    switch (opt) {
		case 'a':
		    opt_fopen[0] = 'a';
		    break;
		case 'i':
		    fprintf(stderr, "ingore interrupt not implemented\n");
		    break;
		case 'h':
		    usage(tee_usage);
		    break;
		default:
		    fprintf(stderr, "tee: invalid option -- %c\n", opt);
		    usage(tee_usage);
	    }
	} else {
	    break;
	}
    }

    /* init FILE pointers */
    FL_init();
    for ( ; i < argc; i++) {
	FL_add(argv[i], opt_fopen);
    }

    /* read and redirect */
    while ((c = (char) getchar()) && (!feof(stdin))) {
	FL_apply(tee_fwrite, c);
    }

    /* clean up */
    FL_apply(tee_fclose, 0);
    exit(0);
}

/* $Id: tee.c,v 1.3 1999/12/10 07:41:03 beppu Exp $ */
