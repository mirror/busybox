/* vi: set sw=4 ts=4: */
/*
 * Mini rmmod implementation for busybox
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
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include "busybox.h"

extern int delete_module(const char * name);


extern int rmmod_main(int argc, char **argv)
{
	int n, ret = EXIT_SUCCESS;
	size_t nmod = 0; /* number of modules */
	size_t pnmod = -1; /* previous number of modules */
	void *buf; /* hold the module names which we ignore but must get */
	size_t bufsize = 0;
	unsigned int flags = O_NONBLOCK|O_EXCL;

	/* Parse command line. */
	while ((n = getopt(argc, argv, "a")) != EOF) {
		switch (n) {
			case 'w':       // --wait
				flags &= ~O_NONBLOCK;
				break;  
			case 'f':       // --force
				flags |= O_TRUNC;
				break;  
			case 'a':
				/* Unload _all_ unused modules via NULL delete_module() call */
				/* until the number of modules does not change */
				buf = xmalloc(bufsize = 256);
				while (nmod != pnmod) {
					if (delete_module(NULL))
						bb_perror_msg_and_die("rmmod");
					pnmod = nmod;
					/* 1 == QM_MODULES */
					if (my_query_module(NULL, 1, &buf, &bufsize, &nmod)) {
						bb_perror_msg_and_die("QM_MODULES");
					}
				}
#ifdef CONFIG_FEATURE_CLEAN_UP
				free(buf);
#endif
				return EXIT_SUCCESS;
			default:
				bb_show_usage();
		}
	}

	if (optind == argc)
		bb_show_usage();

	for (n = optind; n < argc; n++) {
		if (syscall(__NR_delete_module, argv[n], flags) < 0) {
			bb_perror_msg("%s", argv[n]);
			ret = EXIT_FAILURE;
		}
	}

	return(ret);
}
