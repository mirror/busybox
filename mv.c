/*
 * Mini mv implementation for busybox
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


static const char mv_usage[] = "mv SOURCE DEST\n"
"   or: mv SOURCE... DIRECTORY\n\n"
"Rename SOURCE to DEST, or move SOURCE(s) to DIRECTORY.\n";


static const char *srcName;
static const char *destName;
static int dirFlag = FALSE;

static int fileAction(const char *fileName, struct stat* statbuf)
{
    char newdestName[NAME_MAX];

    fprintf(stderr, "srcName='%s'  destName='%s'\n", srcName, destName);
    strcpy(newdestName, destName);
    strcat(newdestName, "/");
    strcat(newdestName, strstr(fileName, fileName));
    fprintf(stderr, "newdestName='%s'\n", newdestName); 
    return (copyFile(fileName, newdestName, TRUE, TRUE));
}


extern int mv_main(int argc, char **argv)
{
    char newdestName[NAME_MAX];
    char *skipName;

    if (argc < 3) {
	usage (mv_usage);
    }
    argc--;
    argv++;

    destName = argv[argc - 1];
    dirFlag = isDirectory(destName);

    if ((argc > 3) && dirFlag==FALSE) {
	fprintf(stderr, "%s: not a directory\n", destName);
	exit (FALSE);
    }
    
    while (argc-- > 1) {
	srcName = *(argv++);
	skipName = strrchr(srcName, '/');
	if (skipName) 
	    skipName++;
	strcpy(newdestName, destName);
	if (dirFlag==TRUE) {
	    strcat(newdestName, "/");
	    if ( skipName != NULL)
		strcat(newdestName, strstr(srcName, skipName));
	    else
		strcat(newdestName, srcName);
	}
	if (isDirectory(srcName)==TRUE && newdestName[strlen(newdestName)] != '/') {
		strcat(newdestName, "/");
	    createPath(newdestName, 0777);
	    fprintf(stderr, "srcName = '%s'\n", srcName);
	    fprintf(stderr, "newdestName = '%s'\n", newdestName);
	    if (recursiveAction(srcName, TRUE, TRUE, FALSE,
				   fileAction, fileAction) == FALSE) 
	    {
		exit( FALSE);
	    }
	    exit( TRUE);
	} else {
	    if (copyFile(srcName, newdestName, FALSE, FALSE)  == FALSE) {
		exit( FALSE);
	    }
	    if (unlink (srcName) < 0) {
		perror (srcName);
		exit( FALSE);
	    }
	}
    }
    exit( TRUE);
}
