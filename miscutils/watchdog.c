/* vi: set sw=4 ts=4: */
/*
 * Mini watchdog implementation for busybox
 *
 * Copyright (C) 2003  Paul Mundt <lethal@linux-sh.org>
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
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "busybox.h"

/* Userspace timer duration, in seconds */
static unsigned int timer_duration = 30;

/* Watchdog file descriptor */
static int fd;

static void watchdog_shutdown(int unused)
{
	write(fd, "V", 1);	/* Magic */
	close(fd);
	exit(0);
}

extern int watchdog_main(int argc, char **argv)
{
	int opt;

	while ((opt = getopt(argc, argv, "t:")) > 0) {
		switch (opt) {
			case 't':
				timer_duration = bb_xgetlarg(optarg, 10, 0, INT_MAX);
				break;
			default:
				bb_show_usage();
		}
	}

	/* We're only interested in the watchdog device .. */
	if (optind < argc - 1 || argc == 1)
		bb_show_usage();

	if (daemon(0, 1) < 0)
		bb_perror_msg_and_die("Failed forking watchdog daemon");

	signal(SIGHUP, watchdog_shutdown);
	signal(SIGINT, watchdog_shutdown);

	fd = bb_xopen(argv[argc - 1], O_WRONLY);

	while (1) {
		/*
		 * Make sure we clear the counter before sleeping, as the counter value
		 * is undefined at this point -- PFM
		 */
		write(fd, "\0", 1);
		sleep(timer_duration);
	}

	watchdog_shutdown(0);

	return EXIT_SUCCESS;
}
