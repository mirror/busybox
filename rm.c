/*
 * Mini rm implementation for busybox
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
#include <errno.h>

static const char* rm_usage = "rm [OPTION]... FILE...\n\n"
"Remove (unlink) the FILE(s).\n\n"
"Options:\n"
"\t-f\tremove existing destinations, never prompt\n"
"\t-r\tremove the contents of directories recursively\n";


static int recursiveFlag = FALSE;
static int forceFlag = FALSE;
static const char *srcName;


static int fileAction(const char *fileName, struct stat* statbuf)
{
    if (unlink( fileName) < 0 ) {
	perror( fileName);
	return ( FALSE);
    }
    return ( TRUE);
}

static int dirAction(const char *fileName, struct stat* statbuf)
{
    if (rmdir( fileName) < 0 ) {
	perror( fileName);
	return ( FALSE);
    }
    return ( TRUE);
}

extern int rm_main(int argc, char **argv)
{
    struct stat statbuf;

    if (argc < 2) {
	usage( rm_usage);
    }
    argc--;
    argv++;

    /* Parse any options */
    while (**argv == '-') {
	while (*++(*argv))
	    switch (**argv) {
	    case 'r':
		recursiveFlag = TRUE;
		break;
	    case 'f':
		forceFlag = TRUE;
		break;
	    default:
		usage( rm_usage);
	    }
	argc--;
	argv++;
    }

    while (argc-- > 0) {
	srcName = *(argv++);
	if (forceFlag == TRUE && lstat(srcName, &statbuf) != 0 && errno == ENOENT) {
	    /* do not reports errors for non-existent files if -f, just skip them */
	}
	else {
	    if (recursiveAction( srcName, recursiveFlag, FALSE, 
			TRUE, fileAction, dirAction) == FALSE) {
		exit( FALSE);
	    }
	}
    }
    exit( TRUE);
}
