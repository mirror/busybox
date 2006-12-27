/* vi: set sw=4 ts=4: */
/*
 * bb_perror_nomsg_and_die implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stddef.h>
#include "libbb.h"

void bb_perror_nomsg_and_die(void)
{
	/* Ignore the gcc warning about a null format string. */
	bb_perror_msg_and_die(NULL);
}
