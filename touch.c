/*
 * Mini touch implementation for busybox
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>
#include <errno.h>


static const char touch_usage[] = "touch [-c] file [file ...]\n\n"
"\tUpdate the last-modified date on the given file[s].\n";



extern int 
touch_main(int argc, char **argv)
{
    int fd;
    int create=TRUE;

    if (argc < 2) {
	usage( touch_usage);
    }
    argc--;
    argv++;

    /* Parse options */
    while (**argv == '-') {
	while (*++(*argv)) switch (**argv) {
	    case 'c':
		create = FALSE;
		break;
	    default:
		fprintf(stderr, "Unknown option: %c\n", **argv);
		exit( FALSE);
	}
	argc--;
	argv++;
    }

    fd = open (*argv, (create==FALSE)? O_RDWR : O_RDWR | O_CREAT, 0644);
    if (fd < 0 ) {
	if (create==FALSE && errno == ENOENT)
	    exit( TRUE);
	else {
	    perror("touch");
	    exit( FALSE);
	}
    }
    close( fd);
    if (utime (*argv, NULL)) {
	perror("touch");
	exit( FALSE);
    }
    else
	exit( TRUE);
}






