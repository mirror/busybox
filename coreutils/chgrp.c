/*
 * Mini chgrp implementation for busybox
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
#include <grp.h>
#include <stdio.h>

const char chgrp_usage[] = "chgrp [OPTION]... GROUP FILE...\n"
    "Change the group membership of each FILE to GROUP.\n"
    "\n\tOptions:\n" "\t-R\tchange files and directories recursively\n";

int chgrp_main(int argc, char **argv)
{
    const char *cp;
    int gid;
    struct group *grp;
    struct stat statBuf;

    if (argc < 2) {
	fprintf(stderr, "Usage: %s %s", *argv, chgrp_usage);
	return 1;
    }
    argc--;
    argv++;

    cp = argv[1];
    if (isDecimal(*cp)) {
	gid = 0;
	while (isDecimal(*cp))
	    gid = gid * 10 + (*cp++ - '0');
	if (*cp) {
	    fprintf(stderr, "Bad gid value\n");
	    return -1;
	}
    } else {
	grp = getgrnam(cp);
	if (grp == NULL) {
	    fprintf(stderr, "Unknown group name\n");
	    return -1;
	}
	gid = grp->gr_gid;
    }
    argc--;
    argv++;
    while (argc-- > 1) {
	argv++;
	if ((stat(*argv, &statBuf) < 0) ||
	    (chown(*argv, statBuf.st_uid, gid) < 0)) {
	    perror(*argv);
	}
    }
    return 1;
}









#if 0
int
recursive(const char *fileName, BOOL followLinks, const char *pattern,
	  int (*fileAction) (const char *fileName,
			     const struct stat * statbuf),
	  int (*dirAction) (const char *fileName,
			    const struct stat * statbuf))

#endif
