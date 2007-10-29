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
	FILE *file;
	struct group gr;

	/* make sure gid and group haven't already been allocated */
	gr.gr_gid = gid;
	gr.gr_name = group;
	xgroup_study(&gr);

	/* add entry to group */
	file = xfopen(bb_path_group_file, "a");
	/* group:passwd:gid:userlist */
	fprintf(file, "%s:x:%u:\n", group, (unsigned)gr.gr_gid);
	if (ENABLE_FEATURE_CLEAN_UP)
		fclose(file);
#if ENABLE_FEATURE_SHADOWPASSWDS
	file = fopen_or_warn(bb_path_gshadow_file, "a");
	if (file) {
		fprintf(file, "%s:!::\n", group);
		if (ENABLE_FEATURE_CLEAN_UP)
			fclose(file);
	}
#endif
}

#if ENABLE_FEATURE_ADDUSER_TO_GROUP
static void add_user_to_group(char **args,
		const char *path,
		FILE *(*fopen_func)(const char *fileName, const char *mode))
{
	char *line;
	int len = strlen(args[1]);
	llist_t *plist = NULL;
	FILE *group_file;

	group_file = fopen_func(path, "r");

	if (!group_file) return;

	while ((line = xmalloc_getline(group_file))) {
		/* Find the group */
		if (!strncmp(line, args[1], len)
		 && line[len] == ':'
		) {
			/* Add the new user */
			line = xasprintf("%s%s%s", line,
						last_char_is(line, ':') ? "" : ",",
						args[0]);
		}
		llist_add_to_end(&plist, line);
	}

	if (ENABLE_FEATURE_CLEAN_UP) {
		fclose(group_file);
		group_file = fopen_func(path, "w");
		while ((line = llist_pop(&plist))) {
			if (group_file)
				fprintf(group_file, "%s\n", line);
			free(line);
		}
		if (group_file)
			fclose(group_file);
	} else {
		group_file = fopen_func(path, "w");
		if (group_file)
			while ((line = llist_pop(&plist)))
				fprintf(group_file, "%s\n", line);
	}
}
#endif

/*
 * addgroup will take a login_name as its first parameter.
 *
 * gid can be customized via command-line parameters.
 * If called with two non-option arguments, addgroup
 * will add an existing user to an existing group.
 */
int addgroup_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int addgroup_main(int argc, char **argv)
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
	argc -= optind;

#if ENABLE_FEATURE_ADDUSER_TO_GROUP
	if (argc == 2) {
		struct group *gr;

		if (option_mask32) {
			/* -g was there, but "addgroup -g num user group"
			 * is a no-no */
			bb_show_usage();
		}

		/* check if group and user exist */
		xuname2uid(argv[0]); /* unknown user: exit */
		xgroup2gid(argv[1]); /* unknown group: exit */
		/* check if user is already in this group */
		gr = getgrnam(argv[1]);
		for (; *(gr->gr_mem) != NULL; (gr->gr_mem)++) {
			if (!strcmp(argv[0], *(gr->gr_mem))) {
				/* user is already in group: do nothing */
				return EXIT_SUCCESS;
			}
		}
		add_user_to_group(argv, bb_path_group_file, xfopen);
#if ENABLE_FEATURE_SHADOWPASSWDS
		add_user_to_group(argv, bb_path_gshadow_file, fopen_or_warn);
#endif /* ENABLE_FEATURE_SHADOWPASSWDS */
	} else
#endif /* ENABLE_FEATURE_ADDUSER_TO_GROUP */
		new_group(argv[0], gid);

	/* Reached only on success */
	return EXIT_SUCCESS;
}
