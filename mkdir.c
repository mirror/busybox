/*
 * Mini mkdir implementation for busybox
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
#include <errno.h>
#include <sys/param.h>

static const char mkdir_usage[] = "Usage: mkdir [OPTION] DIRECTORY...\n"
"Create the DIRECTORY(ies), if they do not already exist\n\n"
"-m\tset permission mode (as in chmod), not rwxrwxrwx - umask\n"
"-p\tno error if existing, make parent directories as needed\n";


static int parentFlag = FALSE;
static mode_t mode = 777;


extern int mkdir_main(int argc, char **argv)
{
    int i=FALSE;
    argc--;
    argv++;

    /* Parse any options */
    while (argc > 0 && **argv == '-') {
	while (i==FALSE && *++(*argv)) {
	    switch (**argv) {
	    case 'm':
		if (--argc == 0)
		    usage( mkdir_usage);
		/* Find the specified modes */
		mode = 0;
		if ( parse_mode(*(++argv), &mode) == FALSE ) {
		    fprintf(stderr, "Unknown mode: %s\n", *argv);
		    exit( FALSE);
		}
		/* Set the umask for this process so it doesn't 
		 * screw up whatever the user just entered. */
		umask(0);
		i=TRUE;
		break;
	    case 'p':
		parentFlag = TRUE;
		break;
	    default:
		usage( mkdir_usage);
	    }
	}
	argc--;
	argv++;
    }


    if (argc < 1) {
	usage( mkdir_usage);
    }

    while (argc > 0) {
	int status;
	struct stat statBuf;
	status=stat(*argv, &statBuf);
	if (status != -1 && status != ENOENT ) {
	    fprintf(stderr, "%s: File exists\n", *argv);
	    exit( FALSE);
	}
	if (parentFlag == TRUE)
	    createPath(*argv, mode);
	else { 
	    if (mkdir (*argv, mode) != 0) {
		perror(*argv);
		exit( FALSE);
	    }
	}
	argc--;
	argv++;
    }
    exit( TRUE);
}


