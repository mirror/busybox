/*
 * Mini chroot implementation for busybox
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
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>


static const char chroot_usage[] = "NEWROOT [COMMAND...]\n"
"Run COMMAND with root directory set to NEWROOT.\n";



int chroot_main(int argc, char **argv)
{
    if ( (argc < 2) || (**(argv+1) == '-') ) {
	usage( chroot_usage);
    }
    argc--;
    argv++;

    if (chroot (*argv) || (chdir ("/"))) {
	fprintf(stderr, "chroot: cannot change root directory to %s: %s\n",
		*argv, strerror(errno));
	exit( FALSE);
    }

    argc--;
    argv++;
    if (argc >= 1) {
	fprintf(stderr, "command: %s\n", *argv);
	execvp (*argv, argv);
    }
    else {
	char *prog;
	prog = getenv ("SHELL");
	if (!prog)
	    prog = "/bin/sh";
	execlp (prog, prog, NULL);
    }
    fprintf(stderr, "chroot: cannot execute %s: %s\n",
		*argv, strerror(errno));
    exit( FALSE);
}

