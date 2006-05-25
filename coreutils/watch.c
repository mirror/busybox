/* vi: set sw=4 ts=4: */
/*
 * Mini watch implementation for busybox
 *
 * Copyright (C) 2001 by Michael Habermann <mhabermann@gmx.de>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 N/A */
/* BB_AUDIT GNU defects -- only option -n is supported. */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Removed dependency on date_main(), added proper error checking, and
 * reduced size.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include "busybox.h"

int watch_main(int argc, char **argv)
{
	int width, len;
	unsigned period = 2;
	char **watched_argv, *header;

	if (argc < 2) bb_show_usage();

	get_terminal_width_height(1, &width, 0);
	header = xzalloc(width--);

	/* don't use getopt, because it permutes the arguments */
	++argv;
	if ((argc > 3) && !strcmp(*argv, "-n")) {
		period = bb_xgetularg10_bnd(argv[1], 1, UINT_MAX);
		argv += 2;
	}
	watched_argv = argv;

	/* create header */

	len = snprintf(header, width, "Every %ds:", period);
	while (*argv && len<width)
		snprintf(header+len, width-len, " %s", *(argv++));

	while (1) {
		char *thyme;
		time_t t;

		time(&t);
		thyme = ctime(&t);
		len = strlen(thyme);
		if (len < width) header[width-len] = 0;
		
		printf("\033[H\033[J%s %s\n", header, thyme);

		waitpid(bb_xspawn(watched_argv),0,0);
		sleep(period);
	}
}
