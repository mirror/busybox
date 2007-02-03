/* vi: set sw=4 ts=4: */
/*
 * deluser (remove lusers from the system ;) for TinyLogin
 *
 * Copyright (C) 1999 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
 * Unified with delgroup by Tito Ragusa <farmatito@tiscali.it>
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 *
 */

#include "busybox.h"

static void del_line_matching(const char *login, const char *filename)
{
	char *line;
	FILE *passwd;
	int len = strlen(login);
	int found = 0;
	llist_t *plist = NULL;

	passwd = fopen_or_warn(filename, "r");
	if (!passwd) return;

	while ((line = xmalloc_fgets(passwd))) {
		if (!strncmp(line, login, len)
		 && line[len] == ':'
		) {
			found++;
			free(line);
		} else {
			llist_add_to_end(&plist, line);
		}
	}

	if (!ENABLE_FEATURE_CLEAN_UP) {
		if (!found) {
			bb_error_msg("can't find '%s' in '%s'", login, filename);
			return;
		}
		passwd = fopen_or_warn(filename, "w");
		if (passwd)
			while ((line = llist_pop(&plist)))
				fputs(line, passwd);
	} else {
		if (!found) {
			bb_error_msg("can't find '%s' in '%s'", login, filename);
			goto clean_up;
		}
		fclose(passwd);
		passwd = fopen_or_warn(filename, "w");
		if (passwd) {
 clean_up:
			while ((line = llist_pop(&plist))) {
				if (found) fputs(line, passwd);
				free(line);
			}
			fclose(passwd);
		}
	}
}

int deluser_main(int argc, char **argv);
int deluser_main(int argc, char **argv)
{
	if (argc != 2)
		bb_show_usage();

	if (ENABLE_DELUSER
	 && (!ENABLE_DELGROUP || applet_name[3] == 'u')
	) {
		del_line_matching(argv[1], bb_path_passwd_file);
		if (ENABLE_FEATURE_SHADOWPASSWDS)
			del_line_matching(argv[1], bb_path_shadow_file);
	}
	del_line_matching(argv[1], bb_path_group_file);
	if (ENABLE_FEATURE_SHADOWPASSWDS)
		del_line_matching(argv[1], bb_path_gshadow_file);

	return EXIT_SUCCESS;
}

