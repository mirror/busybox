/* vi: set sw=4 ts=4: */
/*
 * Mini rmmod implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc.
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
#include <unistd.h>
#define __LIBRARY__



/* And the system call of the day is...  */
_syscall1(int, delete_module, const char *, name)


static const char rmmod_usage[] =
	"rmmod [OPTION]... [MODULE]...\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nUnloads the specified kernel modules from the kernel.\n\n"
	"Options:\n" 
	"\t-a\tTry to remove all unused kernel modules.\n"
#endif
	;



extern int rmmod_main(int argc, char **argv)
{
	if (argc <= 1) {
		usage(rmmod_usage);
	}

	/* Parse any options */
	while (--argc > 0 && **(++argv) == '-') {
		while (*(++(*argv))) {
			switch (**argv) {
			case 'a':
				/* Unload _all_ unused modules via NULL delete_module() call */
				if (delete_module(NULL)) {
					perror("rmmod");
					exit(FALSE);
				}
				exit(TRUE);
			default:
				usage(rmmod_usage);
			}
		}
	}

	while (argc-- > 0) {
		if (delete_module(*argv) < 0) {
			perror(*argv);
		}
		argv++;
	}
	return(TRUE);
}
