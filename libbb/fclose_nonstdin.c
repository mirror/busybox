/* vi: set sw=4 ts=4: */
/*
 * fclose_nonstdin implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* A number of standard utilities can accept multiple command line args
 * of '-' for stdin, according to SUSv3.  So we encapsulate the check
 * here to save a little space.
 */

#include "libbb.h"

int fclose_if_not_stdin(FILE *f)
{
	if (f != stdin) {
		return fclose(f);
	}
	return 0;
}
