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
#include <stdio.h>
#include <unistd.h>


static const char chroot_usage[] = "NEWROOT [COMMAND...]\n"
"Run COMMAND with root directory set to NEWROOT.\n";



int chroot_main(int argc, char **argv)
{
    if (argc < 2) {
	fprintf(stderr, "Usage: %s %s", *argv, chroot_usage);
	return( FALSE);
    }
    argc--;
    argv++;

    fprintf(stderr, "new root: %s\n", *argv);
    
    if (chroot (*argv) || (chdir ("/"))) {
	perror("cannot chroot");
	return( FALSE);
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
	fprintf(stderr, "no command. running: %s\n", prog);
	execlp (prog, prog, NULL);
    }
    perror("cannot exec");
    return(FALSE);
}

