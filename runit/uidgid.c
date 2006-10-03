#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include "uidgid.h"

static unsigned str_chr(const char *s, int c)
{
	const char *t = s;
	while (t[0] && t[0] != (char)c)
		t++;
	return t - s;
}


unsigned uidgid_get(struct uidgid *u, char *ug, unsigned dogrp) {
	char *g = 0;
	struct passwd *pwd = 0;
	struct group *gr = 0;
	int i, d = 0;

	if (dogrp)
		d = str_chr(ug, ':');
	if (ug[d] == ':') {
		ug[d] = 0;
		g = ug + d + 1;
	}
	pwd = getpwnam(ug);
	if (!pwd) {
		if (g) ug[d] = ':';
		return 0;
	}
	if (g) {
		ug[d] = ':';
		for (i = 0; i < 60; ++i) {
			d = str_chr(g, ':');
			if (g[d] == ':') {
				g[d] = 0;
				gr = getgrnam(g);
				if (!gr) {
					g[d] = ':';
					return 0;
				}
				g[d] = ':';
				u->gid[i] = gr->gr_gid;
				g += d+1;
			}
			else {
				gr = getgrnam(g);
				if (!gr) return 0;
				u->gid[i++] = gr->gr_gid;
				break;
			}
		}
		u->gid[i] = 0;
		u->gids = i;
	}
	if (!g) {
		u->gid[0] = pwd->pw_gid;
		u->gids = 1;
	}
	u->uid = pwd->pw_uid;
	return 1;
}
