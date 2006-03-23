/* vi: set sw=4 ts=4: */
/*
 * Mini chown implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
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

/* BB_AUDIT SUSv3 defects - unsupported options -h, -H, -L, and -P. */
/* BB_AUDIT GNU defects - unsupported options -h, -c, -f, -v, and long options. */
/* BB_AUDIT Note: gnu chown does not support -H, -L, or -P. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/chown.html */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "busybox.h"

/* Don't use lchown for glibc older then 2.1.x */
#if (__GLIBC__ <= 2) && (__GLIBC_MINOR__ < 1)
#define lchown	chown
#endif

static long uid;
static long gid;

static int (*chown_func)(const char *, uid_t, gid_t) = chown;

static int fileAction(const char *fileName, struct stat *statbuf, void* junk)
{
	if (chown_func(fileName, uid, (gid == -1) ? statbuf->st_gid : gid) == 0) {
		chmod(fileName, statbuf->st_mode);
		return (TRUE);
	}
	bb_perror_msg("%s", fileName);	/* Avoid multibyte problems. */
	return (FALSE);
}

#define FLAG_R 1
#define FLAG_h 2

int chown_main(int argc, char **argv)
{
	int flags;
	int retval = EXIT_SUCCESS;
	char *groupName;

	flags = bb_getopt_ulflags(argc, argv, "Rh");

	if (flags & FLAG_h) chown_func = lchown;

	if (argc - optind < 2) {
		bb_show_usage();
	}

	argv += optind;

	/* First, check if there is a group name here */
	if ((groupName = strchr(*argv, '.')) == NULL) {
		groupName = strchr(*argv, ':');
	}

	gid = -1;
	if (groupName) {
		*groupName++ = '\0';
		gid = get_ug_id(groupName, my_getgrnam);
	}

	/* Now check for the username */
	uid = get_ug_id(*argv, my_getpwnam);

	++argv;

	/* Ok, ready to do the deed now */
	do {
		if (! recursive_action (*argv, (flags & FLAG_R), FALSE, FALSE,
								fileAction, fileAction, NULL)) {
			retval = EXIT_FAILURE;
		}
	} while (*++argv);

	return retval;
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
