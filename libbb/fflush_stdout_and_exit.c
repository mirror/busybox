/* vi: set sw=4 ts=4: */
/*
 * fflush_stdout_and_exit implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* Attempt to fflush(stdout), and exit with an error code if stdout is
 * in an error state.
 */

#include <stdio.h>
#include <stdlib.h>
#include <libbb.h>

void bb_fflush_stdout_and_exit(int retval)
{
	if (fflush(stdout)) {
		retval = bb_default_error_retval;
	}
	if (die_sleep)
		sleep(die_sleep);
	exit(retval);
}
