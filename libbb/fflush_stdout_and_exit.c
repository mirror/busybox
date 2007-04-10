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

#include "libbb.h"

// TODO: make it safe to call from NOFORK applets
// Currently, it can exit(0). Even if it is made to do longjmp trick
// (see sleep_and_die internals), zero cannot be passed thru this way!

void fflush_stdout_and_exit(int retval)
{
	if (fflush(stdout))
		sleep_and_die();
	exit(retval);
}
