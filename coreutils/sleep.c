/* vi: set sw=4 ts=4: */
/*
 * sleep implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant */
/* BB_AUDIT GNU issues -- fancy version matches except args must be ints. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/sleep.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Rewritten to do proper arg and error checking.
 * Also, added a 'fancy' configuration to accept multiple args with
 * time suffixes for seconds, minutes, hours, and days.
 */

#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include "busybox.h"

#ifdef CONFIG_FEATURE_FANCY_SLEEP
static const struct suffix_mult sfx[] = {
	{ "s", 1 },
	{ "m", 60 },
	{ "h", 60*60 },
	{ "d", 24*60*60 },
	{ NULL, 0 }
};
#endif

int sleep_main(int argc, char **argv);
int sleep_main(int argc, char **argv)
{
	unsigned int duration;

#ifdef CONFIG_FEATURE_FANCY_SLEEP

	if (argc < 2) {
		bb_show_usage();
	}

	++argv;
	duration = 0;
	do {
		duration += xatoul_range_sfx(*argv, 0, UINT_MAX-duration, sfx);
	} while (*++argv);

#else  /* CONFIG_FEATURE_FANCY_SLEEP */

	if (argc != 2) {
		bb_show_usage();
	}

	duration = xatou(argv[1]);

#endif /* CONFIG_FEATURE_FANCY_SLEEP */

	if (sleep(duration)) {
		bb_perror_nomsg_and_die();
	}

	return EXIT_SUCCESS;
}
