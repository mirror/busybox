/* vi: set sw=4 ts=4: */
/*
 * Copyright 2008 Walter Harms (WHarms@bfs.de)
 *
 * Licensed under the GPL v2, see the file LICENSE in this tarball.
 */
#include "libbb.h"
#include "lpr.h"

void parse_prt(const char *buf, struct netprint *netprint)
{
	const char *p;

	if (!buf) {
		buf = getenv("PRINTER");
		if (!buf)
			buf = "lp"; /* "...@localhost:515" is implied */
	}
	p = strchrnul(buf, '@');
	netprint->queue = xstrndup(buf, p - buf);
	if (!*p) /* just queue? example: "lpq -Pcopie" */
		p = "localhost";
	netprint->server = xstrdup(p);

	netprint->lsa = xhost2sockaddr(netprint->server,
			bb_lookup_port(NULL, "tcp", 515));
}
