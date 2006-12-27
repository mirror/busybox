/* vi: set sw=4 ts=4: */
/*
 * Mini hostid implementation for busybox
 *
 * Copyright (C) 2000  Edward Betts <edward@debian.org>.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 N/A -- Matches GNU behavior. */

#include <stdlib.h>
#include <unistd.h>
#include "busybox.h"

int hostid_main(int argc, char ATTRIBUTE_UNUSED **argv)
{
	if (argc > 1) {
		bb_show_usage();
	}

	printf("%lx\n", gethostid());

	fflush_stdout_and_exit(EXIT_SUCCESS);
}
