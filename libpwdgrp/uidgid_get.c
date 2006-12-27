#include "busybox.h"

unsigned uidgid_get(struct bb_uidgid_t *u, const char *ug /*, unsigned dogrp */)
{
	struct passwd *pwd;
	struct group *gr;
	const char *g;

	/* g = 0; if (dogrp) g = strchr(ug, ':'); */
	g = strchr(ug, ':');
	if (g) {
		int sz = (++g) - ug;
		char buf[sz];
		safe_strncpy(buf, ug, sz);
		pwd = getpwnam(buf);
	} else
		pwd = getpwnam(ug);
	if (!pwd)
		return 0;
	u->uid = pwd->pw_uid;
	u->gid = pwd->pw_gid;
	if (g) {
		gr = getgrnam(g);
		if (!gr) return 0;
		u->gid = gr->gr_gid;
	}
	return 1;
}

#if 0
#include <stdio.h>
int main()
{
	unsigned u;
	struct bb_uidgid_t ug;
	u = uidgid_get(&ug, "apache");
	printf("%u = %u:%u\n", u, ug.uid, ug.gid);
	ug.uid = ug.gid = 1111;
	u = uidgid_get(&ug, "apache");
	printf("%u = %u:%u\n", u, ug.uid, ug.gid);
	ug.uid = ug.gid = 1111;
	u = uidgid_get(&ug, "apache:users");
	printf("%u = %u:%u\n", u, ug.uid, ug.gid);
	ug.uid = ug.gid = 1111;
	u = uidgid_get(&ug, "apache:users");
	printf("%u = %u:%u\n", u, ug.uid, ug.gid);
	return 0;
}
#endif
