/* vi: set sw=4 ts=4: */
/*
 * bb_xdaemon.c - a daemon() which dies on failure with error message
 *
 * Copyright (C) 2006 Denis Vlasenko
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <unistd.h>
#include "libbb.h"

#ifndef BB_NOMMU
void bb_xdaemon(int nochdir, int noclose)
{
	if (daemon(nochdir, noclose))
		bb_perror_msg_and_die("daemon");
}
#endif
