/* vi: set sw=4 ts=4: */
/*
 * Mini watchdog implementation for busybox
 *
 * Copyright (C) 2003  Paul Mundt <lethal@linux-sh.org>
 * Copyright (C) 2006  Bernhard Fischer <busybox@busybox.net>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "libbb.h"

#define OPT_FOREGROUND 0x01
#define OPT_TIMER      0x02

static void watchdog_shutdown(int sig ATTRIBUTE_UNUSED)
{
	static const char V = 'V';

	write(3, &V, 1);	/* Magic, see watchdog-api.txt in kernel */
	if (ENABLE_FEATURE_CLEAN_UP)
		close(3);
	exit(EXIT_SUCCESS);
}

int watchdog_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int watchdog_main(int argc, char **argv)
{
	unsigned opts;
	unsigned timer_duration = 30000; /* Userspace timer duration, in milliseconds */
	char *t_arg;

	opt_complementary = "=1"; /* must have 1 argument */
	opts = getopt32(argv, "Ft:", &t_arg);

	if (opts & OPT_TIMER) {
		static const struct suffix_mult suffixes[] = {
			{ "ms", 1 },
			{ "", 1000 },
			{ }
		};
		timer_duration = xatou_sfx(t_arg, suffixes);
	}

	if (!(opts & OPT_FOREGROUND)) {
		bb_daemonize_or_rexec(DAEMON_CHDIR_ROOT, argv);
	}

	bb_signals(BB_FATAL_SIGS, watchdog_shutdown);

	/* Use known fd # - avoid needing global 'int fd' */
	xmove_fd(xopen(argv[argc - 1], O_WRONLY), 3);

// TODO?
//	if (!(opts & OPT_TIMER)) {
//		if (ioctl(fd, WDIOC_GETTIMEOUT, &timer_duration) == 0)
//			timer_duration *= 500;
//		else
//			timer_duration = 30000;
//	}

	while (1) {
		/*
		 * Make sure we clear the counter before sleeping, as the counter value
		 * is undefined at this point -- PFM
		 */
		write(3, "", 1); /* write zero byte */
		usleep(timer_duration * 1000L);
	}
	return EXIT_SUCCESS; /* - not reached, but gcc 4.2.1 is too dumb! */
}
