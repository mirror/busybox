/* vi: set sw=4 ts=4: */
/*
 * Mini watchdog implementation for busybox
 *
 * Copyright (C) 2000  spoon <spoon@ix.netcom.com>.
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

/* getopt not needed */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "busybox.h"

extern int watchdog_main(int argc, char **argv)
{
	int fd;

	if (argc != 2) {
		show_usage();
	}

	if ((fd=open(argv[1], O_WRONLY)) == -1) {
		perror_msg_and_die(argv[1]);
	}

	while (1) {
		sleep(30);
		write(fd, "\0", 1);
	}

	return EXIT_FAILURE;
}
