/* vi: set sw=4 ts=4: */
/*
 * Mini watchdog implementation for busybox
 *
 * Copyright (C) 2003  Paul Mundt <lethal@linux-sh.org>
 * Copyright (C) 2006  Bernhard Fischer <busybox@busybox.net>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "busybox.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#define OPT_FOREGROUND 0x01
#define OPT_TIMER      0x02

/* Watchdog file descriptor */
static int fd;

static void watchdog_shutdown(int ATTRIBUTE_UNUSED unused)
{
	write(fd, "V", 1);	/* Magic, see watchdog-api.txt in kernel */
	close(fd);
	exit(0);
}

int watchdog_main(int argc, char **argv)
{
	unsigned long opts;
	unsigned long timer_duration = 30; /* Userspace timer duration, in seconds */
	char *t_arg;

	opts = bb_getopt_ulflags(argc, argv, "Ft:", &t_arg);

	if (opts & OPT_TIMER)
		timer_duration = bb_xgetlarg(t_arg, 10, 0, INT_MAX);

	/* We're only interested in the watchdog device .. */
	if (optind < argc - 1 || argc == 1)
		bb_show_usage();

#ifdef BB_NOMMU
	if (!(opts & OPT_FOREGROUND))
		vfork_daemon_rexec(0, 1, argc, argv, "-F");
#else
	bb_xdaemon(0, 1);
#endif

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
