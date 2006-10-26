/* vi: set sw=4 ts=4: */
/*
 * Mini dirname implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/dirname.html */

#include <stdio.h>
#include <stdlib.h>
#include "busybox.h"

int dirname_main(int argc, char **argv)
{
	if (argc != 2) {
		bb_show_usage();
	}

	puts(dirname(argv[1]));

	fflush_stdout_and_exit(EXIT_SUCCESS);
}
