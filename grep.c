/*
 * Mini grep implementation for busybox
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
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>


static const char grep_usage[] =
"grep [-ihn]... PATTERN [FILE]...\n"
"Search for PATTERN in each FILE or standard input.\n\n"
"\t-h\tsuppress the prefixing filename on output\n"
"\t-i\tignore case distinctions\n"
"\t-n\tprint line number with output lines\n\n"
"This version of grep matches strings (not full regexps).\n";


/*
 * See if the specified needle is found in the specified haystack.
 */
static int search (const char *haystack, const char *needle, int ignoreCase)
{

    if (ignoreCase == FALSE) {
	haystack = strstr (haystack, needle);
	if (haystack == NULL)
	    return FALSE;
	return TRUE;
    } else {
	int i;
	char needle1[BUF_SIZE];
	char haystack1[BUF_SIZE];

	strncpy( haystack1, haystack, sizeof(haystack1));
	strncpy( needle1, needle, sizeof(needle1));
	for( i=0; i<sizeof(haystack1) && haystack1[i]; i++)
	    haystack1[i]=tolower( haystack1[i]);
	for( i=0; i<sizeof(needle1) && needle1[i]; i++)
	    needle1[i]=tolower( needle1[i]);
	haystack = strstr (haystack1, needle1);
	if (haystack == NULL)
	    return FALSE;
	return TRUE;
    }
}


extern int grep_main (int argc, char **argv)
{
    FILE *fp;
    const char *needle;
    const char *name;
    const char *cp;
    int tellName=TRUE;
    int ignoreCase=FALSE;
    int tellLine=FALSE;
    long line;
    char buf[BUF_SIZE];

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
	    case 'i':
		ignoreCase = TRUE;
		break;

	    case 'h':
		tellName = FALSE;
		break;

	    case 'n':
		tellLine = TRUE;
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

	while (fgets (buf, sizeof (buf), fp)) {
	    line++;
	    cp = &buf[strlen (buf) - 1];

	    if (*cp != '\n')
		fprintf (stderr, "%s: Line too long\n", name);

	    if (search (buf, needle, ignoreCase)==TRUE) {
		if (tellName==TRUE)
		    printf ("%s: ", name);

		if (tellLine==TRUE)
		    printf ("%ld: ", line);

		fputs (buf, stdout);
	    }
	}

	if (ferror (fp))
	    perror (name);

	fclose (fp);
    }
    exit( TRUE);
}


/* END CODE */


