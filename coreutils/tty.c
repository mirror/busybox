/* vi: set sw=4 ts=4: */
/*
 * tty implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/tty.html */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "busybox.h"

int tty_main(int argc, char **argv)
{
	const char *s;
	int silent;		/* Note: No longer relevant in SUSv3. */
	int retval;

	xfunc_error_retval = 2;	/* SUSv3 requires > 1 for error. */

	silent = getopt32(argc, argv, "s");

	/* gnu tty outputs a warning that it is ignoring all args. */
	bb_warn_ignoring_args(argc - optind);

	retval = 0;

	if ((s = ttyname(0)) == NULL) {
	/* According to SUSv3, ttyname can on fail with EBADF or ENOTTY.
	 * We know the file descriptor is good, so failure means not a tty. */
		s = "not a tty";
		retval = 1;
	}

	if (!silent) {
		puts(s);
	}

	fflush_stdout_and_exit(retval);
}
