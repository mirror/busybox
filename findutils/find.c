/* vi: set sw=4 ts=4: */
/*
 * Mini find implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
 *
 * Reworked by David Douthitt <n9ubh@callsign.net> and
 *  Matt Kraai <kraai@alumni.carnegiemellon.edu>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <fnmatch.h>
#include <time.h>
#include <ctype.h>
#include "busybox.h"


static char *pattern;

#ifdef CONFIG_FEATURE_FIND_TYPE
static int type_mask = 0;
#endif

#ifdef CONFIG_FEATURE_FIND_PERM
static char perm_char = 0;
static int perm_mask = 0;
#endif

#ifdef CONFIG_FEATURE_FIND_MTIME
static char mtime_char;
static int mtime_days;
#endif

static int fileAction(const char *fileName, struct stat *statbuf, void* junk)
{
	if (pattern != NULL) {
		const char *tmp = strrchr(fileName, '/');

		if (tmp == NULL)
			tmp = fileName;
		else
			tmp++;
		if (!(fnmatch(pattern, tmp, FNM_PERIOD) == 0))
			goto no_match;
	}
#ifdef CONFIG_FEATURE_FIND_TYPE
	if (type_mask != 0) {
		if (!((statbuf->st_mode & S_IFMT) == type_mask))
			goto no_match;
	}
#endif
#ifdef CONFIG_FEATURE_FIND_PERM
	if (perm_mask != 0) {
		if (!((isdigit(perm_char) && (statbuf->st_mode & 07777) == perm_mask) ||
			 (perm_char == '-' && (statbuf->st_mode & perm_mask) == perm_mask) ||
			 (perm_char == '+' && (statbuf->st_mode & perm_mask) != 0)))
			goto no_match;
	}
#endif
#ifdef CONFIG_FEATURE_FIND_MTIME
	if (mtime_days != 0) {
		time_t file_age = time(NULL) - statbuf->st_mtime;
		time_t mtime_secs = mtime_days * 24 * 60 * 60;
		if (!((isdigit(mtime_char) && mtime_secs >= file_age &&
						mtime_secs < file_age + 24 * 60 * 60) ||
				(mtime_char == '+' && mtime_secs >= file_age) || 
				(mtime_char == '-' && mtime_secs < file_age)))
			goto no_match;
	}
#endif
	puts(fileName);
no_match:
	return (TRUE);
}

#ifdef CONFIG_FEATURE_FIND_TYPE
static int find_type(char *type)
{
	int mask = 0;

	switch (type[0]) {
		case 'b':
			mask = S_IFBLK;
			break;
		case 'c':
			mask = S_IFCHR;
			break;
		case 'd':
			mask = S_IFDIR;
			break;
		case 'p':
			mask = S_IFIFO;
			break;
		case 'f':
			mask = S_IFREG;
			break;
		case 'l':
			mask = S_IFLNK;
			break;
		case 's':
			mask = S_IFSOCK;
			break;
	}

	if (mask == 0 || type[1] != '\0')
		error_msg_and_die("invalid argument `%s' to `-type'", type);

	return mask;
}
#endif

int find_main(int argc, char **argv)
{
	int dereference = FALSE;
	int i, firstopt, status = EXIT_SUCCESS;

	for (firstopt = 1; firstopt < argc; firstopt++) {
		if (argv[firstopt][0] == '-')
			break;
	}

	/* Parse any options */
	for (i = firstopt; i < argc; i++) {
		if (strcmp(argv[i], "-follow") == 0)
			dereference = TRUE;
		else if (strcmp(argv[i], "-print") == 0) {
			;
			}
		else if (strcmp(argv[i], "-name") == 0) {
			if (++i == argc)
				error_msg_and_die("option `-name' requires an argument");
			pattern = argv[i];
#ifdef CONFIG_FEATURE_FIND_TYPE
		} else if (strcmp(argv[i], "-type") == 0) {
			if (++i == argc)
				error_msg_and_die("option `-type' requires an argument");
			type_mask = find_type(argv[i]);
#endif
#ifdef CONFIG_FEATURE_FIND_PERM
		} else if (strcmp(argv[i], "-perm") == 0) {
			char *end;
			if (++i == argc)
				error_msg_and_die("option `-perm' requires an argument");
			perm_mask = strtol(argv[i], &end, 8);
			if (end[0] != '\0')
				error_msg_and_die("invalid argument `%s' to `-perm'", argv[i]);
			if (perm_mask > 07777)
				error_msg_and_die("invalid argument `%s' to `-perm'", argv[i]);
			if ((perm_char = argv[i][0]) == '-')
				perm_mask = -perm_mask;
#endif
#ifdef CONFIG_FEATURE_FIND_MTIME
		} else if (strcmp(argv[i], "-mtime") == 0) {
			char *end;
			if (++i == argc)
				error_msg_and_die("option `-mtime' requires an argument");
			mtime_days = strtol(argv[i], &end, 10);
			if (end[0] != '\0')
				error_msg_and_die("invalid argument `%s' to `-mtime'", argv[i]);
			if ((mtime_char = argv[i][0]) == '-')
				mtime_days = -mtime_days;
#endif
		} else
			show_usage();
	}

	if (firstopt == 1) {
		if (! recursive_action(".", TRUE, dereference, FALSE, fileAction,
					fileAction, NULL))
			status = EXIT_FAILURE;
	} else {
		for (i = 1; i < firstopt; i++) {
			if (! recursive_action(argv[i], TRUE, dereference, FALSE, fileAction,
						fileAction, NULL))
				status = EXIT_FAILURE;
		}
	}

	return status;
}
