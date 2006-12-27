/* vi: set sw=4 ts=4: */
/*
 * warn_ignoring_args implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

void bb_warn_ignoring_args(int n)
{
	if (n) {
		bb_error_msg("ignoring all arguments");
	}
}
