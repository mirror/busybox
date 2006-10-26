/* vi: set sw=4 ts=4: */
/*
 * Mini pwd implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include "busybox.h"

int pwd_main(int argc, char **argv)
{
	char *buf;

	if ((buf = xgetcwd(NULL)) != NULL) {
		puts(buf);
		fflush_stdout_and_exit(EXIT_SUCCESS);
	}

	return EXIT_FAILURE;
}
