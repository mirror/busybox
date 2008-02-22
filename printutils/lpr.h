/* vi: set sw=4 ts=4: */
/*
 * Copyright 2008 Walter Harms (WHarms@bfs.de)
 *
 * Licensed under the GPL v2, see the file LICENSE in this tarball.
 */
#ifndef _LPR_H_
#define _LPR_H_

struct netprint {
	char *queue;
	char *server;
	struct len_and_sockaddr *lsa;
};

void parse_prt(const char *buf, struct netprint *netprint);
#endif
