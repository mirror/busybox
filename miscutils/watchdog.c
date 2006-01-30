/* vi: set sw=4 ts=4: */
/*
 * Mini watchdog implementation for busybox
 *
 * Copyright (C) 2003  Paul Mundt <lethal@linux-sh.org>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
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

static void watchdog_shutdown(int ATTRIBUTE_UNUSED unused)
{
	write(fd, "V", 1);	/* Magic */
	close(fd);
	exit(0);
}

int watchdog_main(int argc, char **argv)
{

	char *t_arg;
	unsigned long flags;
	flags = bb_getopt_ulflags(argc, argv, "t:", &t_arg);
	if (flags & 1)
		timer_duration = bb_xgetlarg(t_arg, 10, 0, INT_MAX);

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
