/* vi: set sw=4 ts=4: */
/*
 * deluser/delgroup implementation for busybox
 *
 * Copyright (C) 1999 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
 * Copyright (C) 2007 by Tito Ragusa <farmatito@tiscali.it>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 *
 */
#include "libbb.h"

int deluser_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int deluser_main(int argc, char **argv)
{
	/* User or group name */
	char *name;
	/* Username (non-NULL only in "delgroup USER GROUP" case) */
	char *member;
	/* Name of passwd or group file */
	const char *pfile;
	/* Name of shadow or gshadow file */
	const char *sfile;
	/* Are we deluser or delgroup? */
	bool do_deluser = (ENABLE_DELUSER && (!ENABLE_DELGROUP || applet_name[3] == 'u'));

	if (geteuid() != 0)
		bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);

	name = argv[1];
	member = NULL;

	switch (argc) {
	case 3:
		if (!ENABLE_FEATURE_DEL_USER_FROM_GROUP || do_deluser)
			break;
		/* It's "delgroup USER GROUP" */
		member = name;
		name = argv[2];
		/* Fallthrough */

	case 2:
		if (do_deluser) {
			/* "deluser USER" */
			xgetpwnam(name); /* bail out if USER is wrong */
			pfile = bb_path_passwd_file;
			if (ENABLE_FEATURE_SHADOWPASSWDS)
				sfile = bb_path_shadow_file;
		} else {
 do_delgroup:
			/* "delgroup GROUP" or "delgroup USER GROUP" */
			xgetgrnam(name); /* bail out if GROUP is wrong */
			if (!member) {
				/* "delgroup GROUP".
				 * If user with the same name exists,
				 * bail out.
				 */
//BUG: check should be done by GID, not by matching name!
//1. find GROUP's GID
//2. check that /etc/passwd doesn't have lines of the form
//   user:pwd:uid:GID:...
//3. bail out if at least one such line exists
				if (getpwnam(name) != NULL)
					bb_error_msg_and_die("'%s' still has '%s' as their primary group!", name, name);
			}
			pfile = bb_path_group_file;
			if (ENABLE_FEATURE_SHADOWPASSWDS)
				sfile = bb_path_gshadow_file;
		}

		/* Modify pfile, then sfile */
		do {
			if (update_passwd(pfile, name, NULL, member) == -1)
				return EXIT_FAILURE;
			if (ENABLE_FEATURE_SHADOWPASSWDS) {
				pfile = sfile;
				sfile = NULL;
			}
		} while (ENABLE_FEATURE_SHADOWPASSWDS && pfile);

		if (ENABLE_DELGROUP && do_deluser) {
			/* "deluser USER" also should try to delete
			 * same-named group. IOW: do "delgroup USER"
			 */
//TODO: check how it actually works in upstream.
//I suspect it is only done if group has no more members.
			do_deluser = 0;
			goto do_delgroup;
		}
		return EXIT_SUCCESS;
	}
	/* Reached only if number of command line args is wrong */
	bb_show_usage();
}
