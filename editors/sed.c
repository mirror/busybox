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
    

static void do_sed(FILE *fp, char *needle, char *newNeedle, int ignoreCase, int printFlag, int quietFlag)
{
    int foundOne=FALSE;
    char haystack[1024];

    while (fgets (haystack, 1023, fp)) {
	foundOne = replace_match(haystack, needle, newNeedle, ignoreCase);
	if (foundOne==TRUE && printFlag==TRUE) {
	    fprintf(stdout, haystack);
	}
	if (quietFlag==FALSE) {
	    fprintf(stdout, haystack);
	}
    }
}

extern int sed_main (int argc, char **argv)
{
    FILE *fp;
    char *needle=NULL, *newNeedle=NULL;
    char *name;
    char *cp;
    int ignoreCase=FALSE;
    int printFlag=FALSE;
    int quietFlag=FALSE;
    int stopNow;

    argc--;
    argv++;
    if (argc < 1) {
	usage(sed_usage);
    }

    if (**argv == '-') {
	argc--;
	cp = *argv++;
	stopNow=FALSE;

	while (*++cp && stopNow==FALSE) {
	    switch (*cp) {
	    case 'n':
		quietFlag = TRUE;
		break;
	    case 'e':
		if (*(cp+1)==0 && --argc < 0) {
		    usage( sed_usage);
		}
		if ( *++cp != 's')
		    cp = *argv++;
		while( *cp ) {
		    if (*cp == 's' && strlen(cp) > 3 && *(cp+1) == '/') {
			char* pos=needle=cp+2;
			for(;;) {
			    pos = strchr(pos, '/');
			    if (pos==NULL) {
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
				usage( sed_usage);
			    }
			    if (*(pos-1) == '\\') {
				pos++;
				continue;
			    }
			    break;
			}
			*pos=0;
			if (pos+2 != 0) {
			    while (*++pos) {
				switch (*pos) {
				    case 'i':
					ignoreCase=TRUE;
					break;
				    case 'p':
					printFlag=TRUE;
					break;
				    case 'g':
					break;
				    default:
					usage( sed_usage);
				}
			    }
			}
		    }
		    cp++;
		}
		//fprintf(stderr, "replace '%s' with '%s'\n", needle, newNeedle);
		stopNow=TRUE;
		break;

	    default:
		usage(sed_usage);
	    }
	}
    }

    if (argc==0) {
	do_sed( stdin, needle, newNeedle, ignoreCase, printFlag, quietFlag);
    } else {
	while (argc-- > 0) {
	    name = *argv++;

	    fp = fopen (name, "r");
	    if (fp == NULL) {
		perror (name);
		continue;
	    }

	    do_sed( fp, needle, newNeedle, ignoreCase, printFlag, quietFlag);

	    if (ferror (fp))
		perror (name);

	    fclose (fp);
	}
    }
    exit( TRUE);
}


/* END CODE */


