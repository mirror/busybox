/* vi: set sw=4 ts=4: */
/*
 * addgroup - add users to /etc/passwd and /etc/shadow
 *
 * Copyright (C) 1999 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 */

#include "busybox.h"

/* make sure gr_name isn't taken, make sure gid is kosher
 * return 1 on failure */
static int group_study(struct group *g)
{
	FILE *etc_group;
	gid_t desired;

	struct group *grp;
	const int max = 65000;

	etc_group = xfopen(bb_path_group_file, "r");

	/* make sure gr_name isn't taken, make sure gid is kosher */
	desired = g->gr_gid;
	while ((grp = fgetgrent(etc_group))) {
		if ((strcmp(grp->gr_name, g->gr_name)) == 0) {
			bb_error_msg_and_die("%s: group already in use", g->gr_name);
		}
		if ((desired) && grp->gr_gid == desired) {
			bb_error_msg_and_die("%d: gid already in use",
							  desired);
		}
		if ((grp->gr_gid > g->gr_gid) && (grp->gr_gid < max)) {
			g->gr_gid = grp->gr_gid;
		}
	}
	fclose(etc_group);

	/* gid */
	if (desired) {
		g->gr_gid = desired;
	} else {
		g->gr_gid++;
	}
	/* return 1; */
	return 0;
}

/* append a new user to the passwd file */
static int addgroup(char *group, gid_t gid, const char *user)
{
	FILE *file;
	struct group gr;

	/* make sure gid and group haven't already been allocated */
	gr.gr_gid = gid;
	gr.gr_name = group;
	if (group_study(&gr))
		return 1;

	/* add entry to group */
	file = xfopen(bb_path_group_file, "a");
	/* group:passwd:gid:userlist */
	fprintf(file, "%s:%s:%d:%s\n", group, "x", gr.gr_gid, user);
	fclose(file);

#if ENABLE_FEATURE_SHADOWPASSWDS
	file = xfopen(bb_path_gshadow_file, "a");
	fprintf(file, "%s:!::\n", group);
	fclose(file);
#endif

	/* return 1; */
	return 0;
}

/*
 * addgroup will take a login_name as its first parameter.
 *
 * gid
 *
 * can be customized via command-line parameters.
 * ________________________________________________________________________ */
int addgroup_main(int argc, char **argv)
{
	char *group;
	gid_t gid = 0;

	/* check for min, max and missing args and exit on error */
	opt_complementary = "-1:?2:?";
	if (getopt32(argc, argv, "g:", &group)) {
		gid = xatoul_range(group, 0, (gid_t)ULONG_MAX);
	}
	/* move past the commandline options */
	argv += optind;

	/* need to be root */
	if (geteuid()) {
		bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);
	}

	/* werk */
	return addgroup(argv[0], gid, (argv[1]) ? argv[1] : "");
}
