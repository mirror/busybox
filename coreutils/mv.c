/*
 * Mini mv implementation for busybox
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
#include <errno.h>

const char mv_usage[] = "source-file [source-file ...] destination-file\n"
    "\n" "\tMove the source files to the destination.\n" "\n";



extern int mv_main (int argc, char **argv)
{
    const char *srcName;
    const char *destName;
    const char *lastArg;
    int dirFlag;

    if (argc < 3) {
	fprintf (stderr, "Usage: %s %s", *argv, mv_usage);
	exit (FALSE);
    }
    lastArg = argv[argc - 1];

    dirFlag = isDirectory (lastArg);

    if ((argc > 3) && !dirFlag) {
	fprintf (stderr, "%s: not a directory\n", lastArg);
	exit (FALSE);
    }

    while (argc-- > 2) {
	srcName = *(++argv);

	if (access (srcName, 0) < 0) {
	    perror (srcName);
	    continue;
	}

	destName = lastArg;

	if (dirFlag==TRUE)
	    destName = buildName (destName, srcName);

	if (rename (srcName, destName) >= 0)
	    continue;

	if (errno != EXDEV) {
	    perror (destName);
	    continue;
	}

	if (!copyFile (srcName, destName, TRUE, FALSE))
	    continue;

	if (unlink (srcName) < 0)
	    perror (srcName);
    }
    exit (TRUE);
}
