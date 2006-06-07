/* vi: set sw=4 ts=4: */
/*
 * bb_xlisten.c - a listen() which dies on failure with error message
 *
 * Copyright (C) 2006 Denis Vlasenko
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <sys/socket.h>
#include "libbb.h"

void bb_xlisten(int s, int backlog)
{
	if (listen(s, backlog))
		bb_perror_msg_and_die("listen");
}
