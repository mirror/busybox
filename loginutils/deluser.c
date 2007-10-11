/* vi: set sw=4 ts=4: */
/*
 * deluser/delgroup implementation for busybox
 *
 * Copyright (C) 1999 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
 * Copyright (C) 2007 by Tito Ragusa <farmatito@tiscali.it>
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 *
 */

#include "libbb.h"

/* Status */
#define STATUS_OK            0
#define NAME_NOT_FOUND       1
#define MEMBER_NOT_FOUND     2

static void del_line_matching(char **args,
		const char *filename,
		FILE *(*fopen_func)(const char *fileName, const char *mode))
{
	FILE *passwd;
	smallint error = NAME_NOT_FOUND;
	char *name = (ENABLE_FEATURE_DEL_USER_FROM_GROUP && args[2]) ? args[2] : args[1];
	char *line, *del;
	char *new = xzalloc(1);

	passwd = fopen_func(filename, "r");
	if (passwd) {
		while ((line = xmalloc_fgets(passwd))) {
			int len = strlen(name);

			if (strncmp(line, name, len) == 0
			 && line[len] == ':'
			) {
				error = STATUS_OK;
				if (ENABLE_FEATURE_DEL_USER_FROM_GROUP) {
					struct group *gr;
					char *p;
					if (args[2]
					 /* There were two args on commandline */
					 && (gr = getgrnam(name))
					 /* The group was not deleted in the meanwhile */
					 && (p = strrchr(line, ':'))
					 /* We can find a pointer to the last ':' */
					) {
						error = MEMBER_NOT_FOUND;
						/* Move past ':' (worst case to '\0') and cut the line */
						p[1] = '\0';
						/* Reuse p */
						for (p = xzalloc(1); *gr->gr_mem != NULL; gr->gr_mem++) {
							/* Add all the other group members */
							if (strcmp(args[1], *gr->gr_mem) != 0) {
								del = p;
								p = xasprintf("%s%s%s", p, p[0] ? "," : "", *gr->gr_mem);
								free(del);
							} else
								error = STATUS_OK;
						}
						/* Recompose the line */
						line = xasprintf("%s%s\n", line, p);
						if (ENABLE_FEATURE_CLEAN_UP) free(p);
					} else
						goto skip;
				}
			}
			del = new;
			new = xasprintf("%s%s", new, line);
			free(del);
 skip:
			free(line);
		}

		if (ENABLE_FEATURE_CLEAN_UP) fclose(passwd);

		if (error) {
			if (ENABLE_FEATURE_DEL_USER_FROM_GROUP && error == MEMBER_NOT_FOUND) {
				/* Set the correct values for error message */
				filename = name;
				name = args[1];
			}
			bb_error_msg("can't find %s in %s", name, filename);
		} else {
			passwd = fopen_func(filename, "w");
			if (passwd) {
				fputs(new, passwd);
				if (ENABLE_FEATURE_CLEAN_UP) fclose(passwd);
			}
		}
	}
	free(new);
}

int deluser_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int deluser_main(int argc, char **argv)
{
	if (argc == 2
	 || (ENABLE_FEATURE_DEL_USER_FROM_GROUP
	    && (applet_name[3] == 'g' && argc == 3))
	) {
		if (geteuid())
			bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);

		if ((ENABLE_FEATURE_DEL_USER_FROM_GROUP && argc != 3)
		 || ENABLE_DELUSER
		 || (ENABLE_DELGROUP && ENABLE_DESKTOP)
		) {
			if (ENABLE_DELUSER
			 && (!ENABLE_DELGROUP || applet_name[3] == 'u')
			) {
				del_line_matching(argv, bb_path_passwd_file, xfopen);
				if (ENABLE_FEATURE_SHADOWPASSWDS)
					del_line_matching(argv, bb_path_shadow_file, fopen_or_warn);
			} else if (ENABLE_DESKTOP && ENABLE_DELGROUP && getpwnam(argv[1]))
				bb_error_msg_and_die("can't remove primary group of user %s", argv[1]);
		}
		del_line_matching(argv, bb_path_group_file, xfopen);
		if (ENABLE_FEATURE_SHADOWPASSWDS)
			del_line_matching(argv, bb_path_gshadow_file, fopen_or_warn);
		return EXIT_SUCCESS;
	} else
		bb_show_usage();
}
