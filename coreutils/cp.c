/*
 * Mini cp implementation for busybox
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
#include <stdio.h>
#include <time.h>
#include <utime.h>
#include <dirent.h>

static const char cp_usage[] = "cp [OPTION]... SOURCE DEST\n"
    "   or: cp [OPTION]... SOURCE... DIRECTORY\n\n"
    "Copy SOURCE to DEST, or multiple SOURCE(s) to DIRECTORY.\n"
    "\n"
    "\t-a\tsame as -dpR\n"
    "\t-d\tpreserve links\n"
    "\t-p\tpreserve file attributes if possible\n"
    "\t-R\tcopy directories recursively\n";


static int recursiveFlag = FALSE;
static int followLinks = FALSE;
static int preserveFlag = FALSE;
static const char *srcName;
static const char *destName;
static int destDirFlag = FALSE;
static int srcDirFlag = FALSE;

static int fileAction(const char *fileName, struct stat* statbuf)
{
    char newdestName[NAME_MAX];
    char* newsrcName = NULL;

    strcpy(newdestName, destName);
    if ( srcDirFlag == TRUE ) {
	if (recursiveFlag!=TRUE ) {
	    fprintf(stderr, "cp: %s: omitting directory\n", srcName);
	    return( TRUE);
	}
	strcat(newdestName, strstr(fileName, srcName) + strlen(srcName));
    } 
    
    if (destDirFlag==TRUE && srcDirFlag == FALSE) {
	if (newdestName[strlen(newdestName)-1] != '/' ) {
	    strcat(newdestName, "/");
	}
	newsrcName = strrchr(srcName, '/');
	if (newsrcName && *newsrcName != '\0')
	    strcat(newdestName, newsrcName);
	else
	    strcat(newdestName, srcName);
    }
    
    return (copyFile(fileName, newdestName, preserveFlag, followLinks));
}

extern int cp_main(int argc, char **argv)
{
    if (argc < 3) {
	usage (cp_usage);
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
		usage (cp_usage);
	    }
	argc--;
	argv++;
    }


    destName = argv[argc - 1];
    destDirFlag = isDirectory(destName);

    if ((argc > 3) && destDirFlag==FALSE) {
	fprintf(stderr, "%s: not a directory\n", destName);
	exit (FALSE);
    }

    while (argc-- > 1) {
	srcName = *(argv++);
	srcDirFlag = isDirectory(srcName);
	if (recursiveAction(srcName, recursiveFlag, followLinks, FALSE,
			       fileAction, fileAction) == FALSE) {
	    exit( FALSE);
	}
    }
    exit( TRUE);
}
