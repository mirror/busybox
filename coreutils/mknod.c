/* vi: set sw=4 ts=4: */
/*
 * Mini mknod implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
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
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include "busybox.h"

int mknod_main(int argc, char **argv)
{
	char *thisarg;
	mode_t mode = 0;
	mode_t perm = 0666;
	dev_t dev = 0;

	argc--;
	argv++;

	/* Parse any options */
	while (argc > 1) {
		if (**argv != '-')
			break;
		thisarg = *argv;
		thisarg++;
		switch (*thisarg) {
		case 'm':
			argc--;
			argv++;
			parse_mode(*argv, &perm);
			umask(0);
			break;
		default:
			show_usage();
		}
		argc--;
		argv++;
	}
	if (argc != 4 && argc != 2) {
		show_usage();
	}
	switch (argv[1][0]) {
	case 'c':
	case 'u':
		mode = S_IFCHR;
		break;
	case 'b':
		mode = S_IFBLK;
		break;
	case 'p':
		mode = S_IFIFO;
		if (argc!=2) {
			show_usage();
		}
		break;
	default:
		show_usage();
	}

	if (mode == S_IFCHR || mode == S_IFBLK) {
		dev = (atoi(argv[2]) << 8) | atoi(argv[3]);
	}

	mode |= perm;

	if (mknod(argv[0], mode, dev) != 0)
		perror_msg_and_die("%s", argv[0]);
	return EXIT_SUCCESS;
}

