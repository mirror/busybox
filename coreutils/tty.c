/* vi: set sw=4 ts=4: */
/*
 * tty implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/tty.html */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "busybox.h"

extern int tty_main(int argc, char **argv)
{
	const char *s;
	int silent;		/* Note: No longer relevant in SUSv3. */
	int retval;

	bb_default_error_retval = 2;	/* SUSv3 requires > 1 for error. */

	silent = bb_getopt_ulflags(argc, argv, "s");

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

	bb_fflush_stdout_and_exit(retval);
}
