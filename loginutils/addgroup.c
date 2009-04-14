/* vi: set sw=4 ts=4: */
/*
 * addgroup - add groups to /etc/group and /etc/gshadow
 *
 * Copyright (C) 1999 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
 * Copyright (C) 2007 by Tito Ragusa <farmatito@tiscali.it>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 */
#include "libbb.h"

static void xgroup_study(struct group *g)
{
	/* Make sure gr_name is unused */
	if (getgrnam(g->gr_name)) {
		goto error;
	}

	/* Check if the desired gid is free
	 * or find the first free one */
	while (1) {
		if (!getgrgid(g->gr_gid)) {
			return; /* found free group: return */
		}
		if (option_mask32) {
			/* -g N, cannot pick gid other than N: error */
			g->gr_name = itoa(g->gr_gid);
			goto error;
		}
		g->gr_gid++;
		if (g->gr_gid <= 0) {
			/* overflowed: error */
			bb_error_msg_and_die("no gids left");
		}
	}

 error:
	/* exit */
	bb_error_msg_and_die("group %s already exists", g->gr_name);
}

/* append a new user to the passwd file */
static void new_group(char *group, gid_t gid)
{
	struct group gr;
	char *p;

	/* make sure gid and group haven't already been allocated */
	gr.gr_gid = gid;
	gr.gr_name = group;
	xgroup_study(&gr);

	/* add entry to group */
	p = xasprintf("x:%u:", gr.gr_gid);
	if (update_passwd(bb_path_group_file, group, p, NULL) < 0)
		exit(EXIT_FAILURE);
	if (ENABLE_FEATURE_CLEAN_UP)
		free(p);
#if ENABLE_FEATURE_SHADOWPASSWDS
	/* Ignore errors: if file is missing we suppose admin doesn't want it */
	update_passwd(bb_path_gshadow_file, group, "!::", NULL);
#endif
}

/*
 * addgroup will take a login_name as its first parameter.
 *
 * gid can be customized via command-line parameters.
 * If called with two non-option arguments, addgroup
 * will add an existing user to an existing group.
 */
int addgroup_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int addgroup_main(int argc UNUSED_PARAM, char **argv)
{
	char *group;
	gid_t gid = 0;

	/* need to be root */
	if (geteuid()) {
		bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);
	}

	/* Syntax:
	 *  addgroup group
	 *  addgroup -g num group
	 *  addgroup user group
	 * Check for min, max and missing args */
	opt_complementary = "-1:?2";
	if (getopt32(argv, "g:", &group)) {
		gid = xatoul_range(group, 0, ((unsigned long)(gid_t)ULONG_MAX) >> 1);
	}
	/* move past the commandline options */
	argv += optind;
	//argc -= optind;

#if ENABLE_FEATURE_ADDUSER_TO_GROUP
	if (argv[1]) {
		struct group *gr;

		if (option_mask32) {
			/* -g was there, but "addgroup -g num user group"
			 * is a no-no */
			bb_show_usage();
		}

		/* check if group and user exist */
		xuname2uid(argv[0]); /* unknown user: exit */
		gr = xgetgrnam(argv[1]); /* unknown group: exit */
		/* check if user is already in this group */
		for (; *(gr->gr_mem) != NULL; (gr->gr_mem)++) {
			if (!strcmp(argv[0], *(gr->gr_mem))) {
				/* user is already in group: do nothing */
				return EXIT_SUCCESS;
			}
		}
		if (update_passwd(bb_path_group_file, argv[1], NULL, argv[0]) < 0) {
			return EXIT_FAILURE;
		}
# if ENABLE_FEATURE_SHADOWPASSWDS
		update_passwd(bb_path_gshadow_file, argv[1], NULL, argv[0]);
# endif
	} else
#endif /* ENABLE_FEATURE_ADDUSER_TO_GROUP */
	{
		die_if_bad_username(argv[0]);
		new_group(argv[0], gid);

	}
	/* Reached only on success */
	return EXIT_SUCCESS;
}
