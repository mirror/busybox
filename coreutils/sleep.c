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

#include "libbb.h"

/* This is a NOFORK applet. Be very careful! */


#if ENABLE_FEATURE_FANCY_SLEEP
static const struct suffix_mult sfx[] = {
	{ "s", 1 },
	{ "m", 60 },
	{ "h", 60*60 },
	{ "d", 24*60*60 },
	{ }
};
#endif

int sleep_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int sleep_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	unsigned duration;

	++argv;
	if (!*argv)
		bb_show_usage();

#if ENABLE_FEATURE_FANCY_SLEEP

	duration = 0;
	do {
		duration += xatoul_range_sfx(*argv, 0, UINT_MAX-duration, sfx);
	} while (*++argv);

#else  /* FEATURE_FANCY_SLEEP */

	duration = xatou(*argv);

#endif /* FEATURE_FANCY_SLEEP */

	if (sleep(duration)) {
		bb_perror_nomsg_and_die();
	}

	return EXIT_SUCCESS;
}
