/*
 * Mini ln implementation for busybox
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
#include <dirent.h>
#include <errno.h>


static const char ln_usage[] = "ln [OPTION] TARGET... LINK_NAME|DIRECTORY\n"
"Create a link named LINK_NAME or DIRECTORY to the specified TARGET\n"
"\nOptions:\n"
"\t-s\tmake symbolic links instead of hard links\n"
"\t-f\tremove existing destination files\n";


static int symlinkFlag = FALSE;
static int removeoldFlag = FALSE;


extern int ln_main(int argc, char **argv)
{
    int status;
    static char* linkName;

    if (argc < 3) {
	usage (ln_usage);
    }
    argc--;
    argv++;

    /* Parse any options */
    while (**argv == '-') {
	while (*++(*argv))
	    switch (**argv) {
	    case 's':
		symlinkFlag = TRUE;
		break;
	    case 'f':
		removeoldFlag = TRUE;
		break;
	    default:
		usage (ln_usage);
	    }
	argc--;
	argv++;
    }


    linkName = argv[argc - 1];

    if ((argc > 3) && !(isDirectory(linkName))) {
	fprintf(stderr, "%s: not a directory\n", linkName);
	exit (FALSE);
    }

    while (argc-- >= 2) {
	if (removeoldFlag==TRUE ) {
	    status = ( unlink(linkName) && errno != ENOENT );
	    if ( status != 0 ) {
		perror(linkName);
		exit( FALSE);
	    }
	}
	if ( symlinkFlag==TRUE)
		status = symlink(*argv, linkName);
	else
		status = link(*argv, linkName);
	if ( status != 0 ) {
	    perror(linkName);
	    exit( FALSE);
	}
    }
    exit( TRUE);
}
