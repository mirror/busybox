/* vi: set sw=4 ts=4: */
/*
 * Mini rmmod implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
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
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include "busybox.h"

extern int delete_module(const char * name);


extern int rmmod_main(int argc, char **argv)
{
	int n, ret = EXIT_SUCCESS;

	/* Parse command line. */
	while ((n = getopt(argc, argv, "a")) != EOF) {
		switch (n) {
			case 'a':
				/* Unload _all_ unused modules via NULL delete_module() call */
				if (delete_module(NULL))
					perror_msg_and_die("rmmod");
				return EXIT_SUCCESS;
			default:
				show_usage();
		}
	}

	if (optind == argc)
			show_usage();

	for (n = optind; n < argc; n++) {
		if (delete_module(argv[n]) < 0) {
			perror_msg("%s", argv[n]);
			ret = EXIT_FAILURE;
		}
	}

	return(ret);
}
