/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdio.h>
#include <unistd.h>
#include "libbb.h"

/*
 * Read all of the supplied buffer from a file.
 * This does multiple reads as necessary.
 * Returns the amount read, or -1 on an error.
 * A short read is returned on an end of file.
 */
ssize_t bb_full_read(int fd, void *buf, size_t len)
{
	ssize_t cc;
	ssize_t total;

	total = 0;

	while (len > 0) {
		cc = safe_read(fd, buf, len);

		if (cc < 0)
			return cc;	/* read() returns -1 on failure. */

		if (cc == 0)
			break;

		buf = ((char *)buf) + cc;
		total += cc;
		len -= cc;
	}

	return total;
}
