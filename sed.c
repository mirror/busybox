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
"Allowed scripts come in the following form:\n\n"
"'s/regexp/replacement/[gp]'\n"
"\tattempt to match regexp against the pattern space\n"
"\tand if successful replaces the matched portion with replacement.\n\n"
"Options:\n"
"-e\tadd the script to the commands to be executed\n"
"-n\tsuppress automatic printing of pattern space\n\n"
#if defined BB_REGEXP
"This version of sed matches full regexps.\n";
#else
"This version of sed matches strings (not full regexps).\n";
#endif




extern int sed_main (int argc, char **argv)
{
    FILE *fp;
    char *needle=NULL, *newNeedle=NULL;
    char *name;
    char *cp;
    int ignoreCase=FALSE;
    int foundOne=FALSE;
    int noprintFlag=FALSE;
    int stopNow;
    char *haystack;

    argc--;
    argv++;
    if (argc < 1) {
	usage(sed_usage);
    }

    if (**argv == '-') {
	argc--;
	cp = *argv++;
	stopNow=FALSE;

	while (*++cp && stopNow==FALSE)
	    switch (*cp) {
	    case 'n':
		noprintFlag = TRUE;
		break;
	    case 'e':
		if (*(cp+1)==0 && --argc < 0) {
		    fprintf(stderr, "A\n");
		    usage( sed_usage);
		}
		cp = *argv++;
		while( *cp ) {
		    if (*cp == 's' && strlen(cp) > 3 && *(cp+1) == '/') {
			char* pos=needle=cp+2;
			for(;;) {
			    pos = strchr(pos, '/');
			    if (pos==NULL) {
				fprintf(stderr, "B\n");
				usage( sed_usage);
			    }
			    if (*(pos-1) == '\\') {
				pos++;
				continue;
			    }
			    break;
			}
			*pos=0;
			newNeedle=++pos;
			for(;;) {
			    pos = strchr(pos, '/');
			    if (pos==NULL) {
				fprintf(stderr, "C\n");
				usage( sed_usage);
			    }
			    if (*(pos-1) == '\\') {
				pos++;
				continue;
			    }
			    break;
			}
			*pos=0;
		    }
		    cp++;
		}
		fprintf(stderr, "replace '%s' with '%s'\n", needle, newNeedle);
		stopNow=TRUE;
		break;

	    default:
		fprintf(stderr, "D\n");
		usage(sed_usage);
	    }
    }

    fprintf(stderr, "argc=%d\n", argc);
    while (argc-- > 0) {
	name = *argv++;

	fp = fopen (name, "r");
	if (fp == NULL) {
	    perror (name);
	    continue;
	}
	fprintf(stderr, "filename is '%s'\n", name);

	haystack = (char*)malloc( 80);
	while (fgets (haystack, sizeof (haystack), fp)) {

	    foundOne = replace_match(haystack, needle, newNeedle, ignoreCase);
	    if (noprintFlag==TRUE && foundOne==TRUE)
		fputs (haystack, stdout);
	    else
		fputs (haystack, stdout);
	    /* Avoid any mem leaks */
	    free(haystack);
	    haystack = (char*)malloc( BUF_SIZE);
	}

	if (ferror (fp))
	    perror (name);

	fclose (fp);
    }
    exit( TRUE);
}


/* END CODE */


