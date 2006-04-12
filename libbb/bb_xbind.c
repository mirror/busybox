/* vi: set sw=4 ts=4: */
/*
 * bb_xbind.c - a bind() which dies on failure with error message
 *
 * Copyright (C) 2006 Denis Vlasenko
 *
 * Licensed under LGPL, see file docs/lesser.txt in this tarball for details.
 */
#include <sys/types.h>
#include <sys/socket.h>
#include "libbb.h"

void bb_xbind(int sockfd, struct sockaddr *my_addr, socklen_t addrlen)
{
	if (bind(sockfd, my_addr, addrlen))
		bb_perror_msg_and_die("bind");
}

