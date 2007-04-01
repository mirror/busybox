/* Based on ipsvd utilities written by Gerrit Pape <pape@smarden.org>
 * which are released into public domain by the author.
 * Homepage: http://smarden.sunsite.dk/ipsvd/
 *
 * Copyright (C) 2007 Denis Vlasenko.
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include "ipsvd_perhost.h"

static struct hcc *cc;
static unsigned cclen;

/* to be optimized */

void ipsvd_perhost_init(unsigned c)
{
//	free(cc);
	cc = xzalloc(c * sizeof(*cc));
	cclen = c;
}

unsigned ipsvd_perhost_add(const char *ip, unsigned maxconn, struct hcc **hccpp)
{
	unsigned i;
	unsigned conn = 1;
	int p = -1;

	for (i = 0; i < cclen; ++i) {
		if (cc[i].ip[0] == 0) {
			if (p == -1) p = i;
    			continue;
		}
		if (strncmp(cc[i].ip, ip, sizeof(cc[i].ip)) == 0) {
			conn++;
			continue;
		}
	}
	if (p == -1) return 0;
	if (conn <= maxconn) {
		strcpy(cc[p].ip, ip);
		*hccpp = &cc[p];
	}
	return conn;
}

void ipsvd_perhost_remove(int pid)
{
	unsigned i;
	for (i = 0; i < cclen; ++i) {
		if (cc[i].pid == pid) {
			cc[i].ip[0] = 0;
			cc[i].pid = 0;
			return;
		}
	}
}

//void ipsvd_perhost_free(void)
//{
//	free(cc);
//}
