/*
 * Mini ln implementation for busybox
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


static const char ln_usage[] = "ln [-s] [-f] original-name additional-name\n"
"\n"
"\tAdd a new name that refers to the same file as \"original-name\"\n"
"\n"
"\t-s:\tUse a \"symbolic\" link, instead of a \"hard\" link.\n"
"\t-f:\tRemove existing destination files.\n";


static int symlinkFlag = FALSE;
static int removeoldFlag = FALSE;
static const char *destName;


extern int ln_main(int argc, char **argv)
{
    int status;
    char newdestName[NAME_MAX];

    if (argc < 3) {
	fprintf(stderr, "Usage: %s", ln_usage);
	exit (FALSE);
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
		fprintf(stderr, "Usage: %s\n", ln_usage);
		exit(FALSE);
	    }
	argc--;
	argv++;
    }


    destName = argv[argc - 1];

    if ((argc > 3) && !(isDirectory(destName))) {
	fprintf(stderr, "%s: not a directory\n", destName);
	exit (FALSE);
    }

    while (argc-- >= 2) {
	strcpy(newdestName, destName);
	strcat(newdestName, (*argv)+(strlen(*(++argv))));
	
	if (removeoldFlag==TRUE ) {
	    status = ( unlink(newdestName) && errno != ENOENT );
	    if ( status != 0 ) {
		perror(newdestName);
		exit( FALSE);
	    }
	}
	if ( symlinkFlag==TRUE)
		status = symlink(*argv, newdestName);
	else
		status = link(*argv, newdestName);
	if ( status != 0 ) {
	    perror(newdestName);
	    exit( FALSE);
	}
    }
    exit( TRUE);
}
