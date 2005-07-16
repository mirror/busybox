/* vi: set sw=4 ts=4: */
/*
 * pidof implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999-2002 by Erik Andersen <andersee@debian.org>
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


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include "busybox.h"


extern int pidof_main(int argc, char **argv)
{
	int opt, n = 0;


	/* do normal option parsing */
	while ((opt = getopt(argc, argv, "ne:f:")) > 0) {
		switch (opt) {
#if 0
			case 'g':
				break;
			case 'e':
				break;
#endif
			default:
				show_usage();
		}
	}

	/* if we didn't get a process name, then we need to choke and die here */
	if (argv[optind] == NULL)
		show_usage();

	/* Looks like everything is set to go. */
	while(optind < argc) {
		long* pidList;

		pidList = find_pid_by_name( argv[optind]);
		if (!pidList || *pidList<=0) {
			break;
		}

		for(; pidList && *pidList!=0; pidList++) {
			printf("%s%ld", (n++ ? " " : ""), (long)*pidList);
		}
		/* Note that we don't bother to free the memory
		 * allocated in find_pid_by_name().  It will be freed
		 * upon exit, so we can save a byte or two */
		optind++;
	}
	printf("\n");

	return EXIT_SUCCESS;
}
