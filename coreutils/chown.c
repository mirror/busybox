/* vi: set sw=4 ts=4: */
/*
 * Mini chown implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 defects - unsupported options -h, -H, -L, and -P. */
/* BB_AUDIT GNU defects - unsupported options -h, -c, -f, -v, and long options. */
/* BB_AUDIT Note: gnu chown does not support -H, -L, or -P. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/chown.html */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "busybox.h"

static uid_t uid = -1;
static gid_t gid = -1;

static int (*chown_func)(const char *, uid_t, gid_t) = chown;

static int fileAction(const char *fileName, struct stat *statbuf,
		void ATTRIBUTE_UNUSED *junk)
{
	if (!chown_func(fileName,
				(uid == (uid_t)-1) ? statbuf->st_uid : uid,
				(gid == (gid_t)-1) ? statbuf->st_gid : gid)) {
		return TRUE;
	}
	bb_perror_msg("%s", fileName);	/* A filename could have % in it... */
	return FALSE;
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

	/* Check for the username and groupname */
	if (groupName) {
		*groupName++ = '\0';
		gid = get_ug_id(groupName, bb_xgetgrnam);
	}
	if (--groupName != *argv) uid = get_ug_id(*argv, bb_xgetpwnam);
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
