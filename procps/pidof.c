/* vi: set sw=4 ts=4: */
/*
 * pidof implementation for busybox
 *
 * Copyright (C) 1999-2003 by Erik Andersen <andersen@codepoet.org>
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
	int single_flag = 0;
	int fail = 1;

	/* do normal option parsing */
	while ((opt = getopt(argc, argv, "s")) > 0) {
		switch (opt) {
			case 's':
				single_flag = 1;
				break;
			default:
				bb_show_usage();
		}
	}

	/* Looks like everything is set to go. */
	while(optind < argc) {
		long *pidList;
		long *pl;

		pidList = find_pid_by_name(argv[optind]);
		for(pl = pidList; *pl > 0; pl++) {
			printf("%s%ld", (n++ ? " " : ""), *pl);
			fail = 0;
			if (single_flag)
				break;
		}
		free(pidList);
		optind++;

	}
	printf("\n");

	return fail ? EXIT_FAILURE : EXIT_SUCCESS;
}
