/*
 * Mini cp implementation for busybox
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
#include <time.h>
#include <utime.h>
#include <dirent.h>

const char cp_usage[] = "cp [OPTION]... SOURCE DEST\n"
    "   or: cp [OPTION]... SOURCE... DIRECTORY\n"
    "Copy SOURCE to DEST, or multiple SOURCE(s) to DIRECTORY.\n"
    "\n"
    "\t-a\tsame as -dpR\n"
    "\t-d\tpreserve links\n"
    "\t-p\tpreserve file attributes if possable\n"
    "\t-R\tcopy directories recursively\n";


static int recursiveFlag = FALSE;
static int followLinks = FALSE;
static int preserveFlag = FALSE;
static const char *srcName;
static const char *destName;


static int fileAction(const char *fileName)
{
    char newdestName[NAME_MAX];
    strcpy(newdestName, destName);
    strcat(newdestName, fileName+(strlen(srcName)));
    return (copyFile(fileName, newdestName, preserveFlag, followLinks));
}

extern int cp_main(int argc, char **argv)
{

    int dirFlag;

    if (argc < 3) {
	fprintf(stderr, "Usage: %s", cp_usage);
	exit (FALSE);
    }
    argc--;
    argv++;

    /* Parse any options */
    while (**argv == '-') {
	while (*++(*argv))
	    switch (**argv) {
	    case 'a':
		followLinks = TRUE;
		preserveFlag = TRUE;
		recursiveFlag = TRUE;
		break;
	    case 'd':
		followLinks = TRUE;
		break;
	    case 'p':
		preserveFlag = TRUE;
		break;
	    case 'R':
		recursiveFlag = TRUE;
		break;
	    default:
		fprintf(stderr, "Usage: %s\n", cp_usage);
		exit(FALSE);
	    }
	argc--;
	argv++;
    }


    destName = argv[argc - 1];

    dirFlag = isDirectory(destName);

    if ((argc > 3) && !dirFlag) {
	fprintf(stderr, "%s: not a directory\n", destName);
	exit (FALSE);
    }

    while (argc-- >= 2) {
	srcName = *(argv++);
	exit( recursiveAction(srcName, recursiveFlag, followLinks,
			       fileAction, fileAction));
    }
    exit( TRUE);
}
