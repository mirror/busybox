/* vi: set sw=4 ts=4: */
/*
 * Mini mkfifo implementation for busybox
 *
 * Copyright (C) 1999 by Randolph Chung <tausq@debian.org>
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
#include <sys/types.h>
#include <errno.h>
#include <stdlib.h>
#include "busybox.h"

extern int mkfifo_main(int argc, char **argv)
{
	char *thisarg;
	mode_t mode = 0666;

	argc--;
	argv++;

	/* Parse any options */
	while (argc > 1) {
		if (**argv != '-')
			show_usage();
		thisarg = *argv;
		thisarg++;
		switch (*thisarg) {
		case 'm':
			argc--;
			argv++;
			parse_mode(*argv, &mode);
			break;
		default:
			show_usage();
		}
		argc--;
		argv++;
	}
	if (argc < 1 || *argv[0] == '-')
		show_usage();
	if (mkfifo(*argv, mode) < 0)
		perror_msg_and_die("mkfifo");
	return EXIT_SUCCESS;
}
