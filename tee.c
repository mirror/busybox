/* vi: set sw=4 ts=4: */
/*
 * Mini tee implementation for busybox
 *
 *
 * Copyright (C) 1999,2000 by Lineo, inc.
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
#include <errno.h>
#include <stdio.h>

static const char tee_usage[] =
	"tee [OPTION]... [FILE]...\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nCopy standard input to each FILE, and also to standard output.\n\n"
	"Options:\n" "\t-a\tappend to the given FILEs, do not overwrite\n"
#if 0
	"\t-i\tignore interrupt signals\n"
#endif
#endif
;


/* FileList _______________________________________________________________ */

#define FL_MAX	1024
static FILE **FileList;
static int FL_end;

typedef void (FL_Function) (FILE * file, char c);


/* apply a function to everything in FileList */
static void FL_apply(FL_Function * f, char c)
{
	int i;

	for (i = 0; i <= FL_end; i++) {
		f(FileList[i], c);
	}
}

/* FL_Function for writing to files*/
static void tee_fwrite(FILE * file, char c)
{
	fputc(c, file);
}

/* FL_Function for closing files */
static void tee_fclose(FILE * file, char c)
{
	fclose(file);
}

/* ________________________________________________________________________ */

/* BusyBoxed tee(1) */
int tee_main(int argc, char **argv)
{
	int i;
	char c;
	char opt;
	char opt_fopen[2] = "w";
	FILE *file;

	/* parse argv[] */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			opt = argv[i][1];
			switch (opt) {
			case 'a':
				opt_fopen[0] = 'a';
				break;
#if 0
			case 'i':
				fprintf(stderr, "ignore interrupt not implemented\n");
				break;
#endif
			default:
				usage(tee_usage);
			}
		} else {
			break;
		}
	}

	/* init FILE pointers */
	FileList = calloc(FL_MAX, sizeof(FILE*));
	if (!FileList) {
		fprintf(stderr, "tee: %s\n", strerror(errno));
		exit(1);
	}
	FL_end = 0;
	FileList[0] = stdout;
	for (; i < argc; i++) {
		/* add a file to FileList */
		file = fopen(argv[i], opt_fopen);
		if (!file) {
			continue;
		}
		if (FL_end < FL_MAX) {
			FileList[++FL_end] = file;
		}
	}

	/* read and redirect */
	while ((c = (char) getchar()) && (!feof(stdin))) {
		FL_apply(tee_fwrite, c);
	}

	/* clean up */
	FL_apply(tee_fclose, 0);
	/* Don't bother to close files  Exit does that 
	 * automagically, so we can save a few bytes */
	/* free(FileList); */
	exit(0);
}

/* $Id: tee.c,v 1.10 2000/05/12 19:41:47 erik Exp $ */
