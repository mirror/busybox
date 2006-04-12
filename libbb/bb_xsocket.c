/* vi: set sw=4 ts=4: */
/*
 * bb_xsocket.c - a socket() which dies on failure with error message
 *
 * Copyright (C) 2006 Denis Vlasenko
 *
 * Licensed under LGPL, see file docs/lesser.txt in this tarball for details.
 */
#include <sys/socket.h>
#include "libbb.h"

int bb_xsocket(int domain, int type, int protocol)
{
	int r = socket(domain, type, protocol);
	if (r < 0)
		bb_perror_msg_and_die("socket");
	return r;
}
