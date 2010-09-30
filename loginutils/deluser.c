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
	if (argc != 2
	 && (!ENABLE_FEATURE_DEL_USER_FROM_GROUP
	    || applet_name[3] != 'g'
	    || argc != 3)
	) {
		bb_show_usage();
	}

	if (geteuid())
		bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);

	if (ENABLE_DELUSER && applet_name[3] == 'u') {
		/* deluser USER */
		if (update_passwd(bb_path_passwd_file, argv[1], NULL, NULL) < 0)
			return EXIT_FAILURE;
		if (ENABLE_FEATURE_SHADOWPASSWDS)
			if (update_passwd(bb_path_shadow_file, argv[1], NULL, NULL) < 0)
				return EXIT_FAILURE;
	} else if (ENABLE_DELGROUP) {
		/* delgroup ... */
		if (!ENABLE_FEATURE_DEL_USER_FROM_GROUP || argc != 3) {
			/* delgroup GROUP */
			if (update_passwd(bb_path_group_file, argv[1], NULL, NULL) < 0)
				return EXIT_FAILURE;
			if (ENABLE_FEATURE_SHADOWPASSWDS)
				if (update_passwd(bb_path_gshadow_file, argv[1], NULL, NULL) < 0)
					return EXIT_FAILURE;
		} else {
			/* delgroup USER GROUP */
			if (update_passwd(bb_path_group_file, argv[2], NULL, argv[1]) < 0)
				return EXIT_FAILURE;
			if (ENABLE_FEATURE_SHADOWPASSWDS)
				if (update_passwd(bb_path_gshadow_file, argv[2], NULL, argv[1]) < 0)
					return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}
