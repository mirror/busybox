/*
 * Mini sed implementation for busybox
 *
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
#include "regexp.h"
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>

static const char sed_usage[] = 
"sed [-n] [-e script] [file...]\n"
"Allowed scripts come in two forms:\n"
"'/regexp/[gp]'\n"
"\tattempt to match regexp against the pattern space\n"
"'s/regexp/replacement/[gp]'\n"
"\tattempt to match regexp against the pattern space\n"
"\tand if successful replaces the matched portion with replacement."
"Options:\n"
"-e\tadd the script to the commands to be executed\n"
"-n\tsuppress automatic printing of pattern space\n\n"
#if defined BB_REGEXP
"This version of sed matches full regexps.\n";
#else
"This version of sed matches strings (not full regexps).\n";
#endif


static int replaceFlag = FALSE;
static int noprintFlag = FALSE;


extern int sed_main (int argc, char **argv)
{
    FILE *fp;
    const char *needle;
    const char *name;
    const char *cp;
    int tellName=TRUE;
    int ignoreCase=FALSE;
    int tellLine=FALSE;
    long line;
    char haystack[BUF_SIZE];

    ignoreCase = FALSE;
    tellLine = FALSE;

    argc--;
    argv++;
    if (argc < 1) {
	usage(grep_usage);
    }

    if (**argv == '-') {
	argc--;
	cp = *argv++;

	while (*++cp)
	    switch (*cp) {
	    case 'n':
		noprintFlag = TRUE;
		break;
	    case 'e':
		if (*(*argv)+1 != '\'' && **argv != '\"') {
		    if (--argc == 0)
			usage( mkdir_usage);
		    ++argv;
		    if (*(*argv)+1 != '\'' && **argv != '\"') {
			usage( mkdir_usage);
		}
		/* Find the specified modes */
		mode = 0;
		if ( parse_mode(*(++argv), &mode) == FALSE ) {
		    fprintf(stderr, "Unknown mode: %s\n", *argv);
		    exit( FALSE);
		}
		break;

	    default:
		usage(grep_usage);
	    }
    }

    needle = *argv++;
    argc--;

    while (argc-- > 0) {
	name = *argv++;

	fp = fopen (name, "r");
	if (fp == NULL) {
	    perror (name);
	    continue;
	}

	line = 0;

	while (fgets (haystack, sizeof (haystack), fp)) {
	    line++;
	    cp = &haystack[strlen (haystack) - 1];

	    if (*cp != '\n')
		fprintf (stderr, "%s: Line too long\n", name);

	    if (find_match(haystack, needle, ignoreCase) == TRUE) {
		if (tellName==TRUE)
		    printf ("%s: ", name);

		if (tellLine==TRUE)
		    printf ("%ld: ", line);

		fputs (haystack, stdout);
	    }
	}

	if (ferror (fp))
	    perror (name);

	fclose (fp);
    }
    exit( TRUE);
}


/* END CODE */


