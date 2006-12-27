/* vi: set sw=4 ts=4: */
/*
 * Mini whoami implementation for busybox
 *
 * Copyright (C) 2000  Edward Betts <edward@debian.org>.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 N/A -- Matches GNU behavior. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "busybox.h"

int whoami_main(int argc, char **argv)
{
	if (argc > 1)
		bb_show_usage();

	puts(bb_getpwuid(NULL, geteuid(), -1));
	/* exits on error */
	fflush_stdout_and_exit(EXIT_SUCCESS);
}
