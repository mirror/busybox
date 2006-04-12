/* vi: set sw=4 ts=4: */
/*
 * bb_xdaemon.c - a daemon() which dies on failure with error message
 *
 * Copyright (C) 2006 Denis Vlasenko
 *
 * Licensed under LGPL, see file docs/lesser.txt in this tarball for details.
 */
#include <unistd.h>
#include "libbb.h"

void bb_xdaemon(int nochdir, int noclose)
{
	if (daemon(nochdir, noclose))
		bb_perror_msg_and_die("daemon");
}

