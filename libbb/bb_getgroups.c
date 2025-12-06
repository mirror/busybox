/*
 * Utility routines.
 *
 * Copyright (C) 2017 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

//kbuild:lib-y += bb_getgroups.o

#include "libbb.h"

gid_t* FAST_FUNC bb_getgroups(int *ngroups, gid_t *group_array)
{
	int n = ngroups ? *ngroups : 0;

	/* getgroups may be a bit expensive, try to use it only once */
	if (n < 32)
		n = 32;

	for (;;) {
// FIXME: ash tries so hard to not die on OOM (when we are called from test),
// and we spoil it with just one xrealloc here
		group_array = xrealloc(group_array, (n+1) * sizeof(group_array[0]));
		n = getgroups(n, group_array);
		/*
		 * If buffer is too small, kernel does not return new_n > n.
		 * It returns -1 and EINVAL:
		 */
		if (n >= 0) {
			/* Terminator for bb_getgroups(NULL, NULL) usage */
			group_array[n] = (gid_t) -1;
			break;
		}
		if (errno == EINVAL) { /* too small? */
			/* This is the way to ask kernel how big the array is */
			n = getgroups(0, group_array);
			continue;
		}
		/* Some other error (should never happen on Linux) */
		bb_simple_perror_msg_and_die("getgroups");
	}

	if (ngroups)
		*ngroups = n;
	return group_array;
}

uid_t FAST_FUNC get_cached_euid(uid_t *euid)
{
	if (*euid == (uid_t)-1)
		*euid = geteuid();
	return *euid;
}

gid_t FAST_FUNC get_cached_egid(gid_t *egid)
{
	if (*egid == (gid_t)-1)
		*egid = getegid();
	return *egid;
}

/* Return non-zero if GID is in our supplementary group list. */
int FAST_FUNC is_in_supplementary_groups(struct cached_groupinfo *groupinfo, gid_t gid)
{
	int i;
	int ngroups;
	gid_t *group_array;

	if (groupinfo->ngroups == 0)
		groupinfo->supplementary_array = bb_getgroups(&groupinfo->ngroups, NULL);
	ngroups = groupinfo->ngroups;
	group_array = groupinfo->supplementary_array;

	/* Search through the list looking for GID. */
	for (i = 0; i < ngroups; i++)
		if (gid == group_array[i])
			return 1;

	return 0;
}
