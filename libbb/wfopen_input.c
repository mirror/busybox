/* vi: set sw=4 ts=4: */
/*
 * wfopen_input implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* A number of applets need to open a file for reading, where the filename
 * is a command line arg.  Since often that arg is '-' (meaning stdin),
 * we avoid testing everywhere by consolidating things in this routine.
 *
 * Note: we also consider "" to mean stdin (for 'cmp' at least).
 */

#include "libbb.h"

FILE *fopen_or_warn_stdin(const char *filename)
{
	FILE *fp = stdin;

	if (filename != bb_msg_standard_input
	 && filename[0]
	 && NOT_LONE_DASH(filename)
	) {
		fp = fopen_or_warn(filename, "r");
	}

	return fp;
}
