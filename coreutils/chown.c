/* vi: set sw=4 ts=4: */
/*
 * Mini chown implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 defects - unsupported options -H, -L, and -P. */
/* BB_AUDIT GNU defects - unsupported long options. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/chown.html */

#include "busybox.h"

static struct bb_uidgid_t ugid = { -1, -1 };

static int (*chown_func)(const char *, uid_t, gid_t) = chown;

#define OPT_RECURSE (option_mask32 & 1)
#define OPT_NODEREF (option_mask32 & 2)
#define OPT_VERBOSE (USE_DESKTOP(option_mask32 & 4) SKIP_DESKTOP(0))
#define OPT_CHANGED (USE_DESKTOP(option_mask32 & 8) SKIP_DESKTOP(0))
#define OPT_QUIET   (USE_DESKTOP(option_mask32 & 0x10) SKIP_DESKTOP(0))
#define OPT_STR     ("Rh" USE_DESKTOP("vcf"))

/* TODO:
 * -H if a command line argument is a symbolic link to a directory, traverse it
 * -L traverse every symbolic link to a directory encountered
 * -P do not traverse any symbolic links (default)
 */

static int fileAction(const char *fileName, struct stat *statbuf,
		void ATTRIBUTE_UNUSED *junk, int depth)
{
	// TODO: -H/-L/-P
	// if (depth ... && S_ISLNK(statbuf->st_mode)) ....

	if (!chown_func(fileName,
			(ugid.uid == (uid_t)-1) ? statbuf->st_uid : ugid.uid,
			(ugid.gid == (gid_t)-1) ? statbuf->st_gid : ugid.gid)
	) {
		if (OPT_VERBOSE
		 || (OPT_CHANGED && (statbuf->st_uid != ugid.uid || statbuf->st_gid != ugid.gid))
		) {
			printf("changed ownership of '%s' to %u:%u\n",
					fileName, ugid.uid, ugid.gid);
		}
		return TRUE;
	}
	if (!OPT_QUIET)
		bb_perror_msg("%s", fileName);	/* A filename can have % in it... */
	return FALSE;
}

int chown_main(int argc, char **argv)
{
	char *groupName;
	int retval = EXIT_SUCCESS;

	opt_complementary = "-2";
	getopt32(argc, argv, OPT_STR);
	argv += optind;

	if (OPT_NODEREF) chown_func = lchown;

	/* First, check if there is a group name here */
	groupName = strchr(*argv, '.'); /* deprecated? */
	if (!groupName)
		groupName = strchr(*argv, ':');
	else
		*groupName = ':'; /* replace '.' with ':' */

	/* First, try parsing "user[:[group]]" */
	if (!groupName) { /* "user" */
		ugid.uid = get_ug_id(*argv, xuname2uid);
	} else if (groupName == *argv) { /* ":group" */
		ugid.gid = get_ug_id(groupName + 1, xgroup2gid);
	} else {
		if (!groupName[1]) /* "user:" */
			*groupName = '\0';
		if (!get_uidgid(&ugid, *argv, 1))
			bb_error_msg_and_die("unknown user/group %s", *argv);
	}

	/* Ok, ready to do the deed now */
	argv++;
	do {
		if (!recursive_action(*argv,
				OPT_RECURSE,	// recurse
				FALSE,          // follow links: TODO: -H/-L/-P
				FALSE,          // depth first
				fileAction,     // file action
				fileAction,     // dir action
				NULL,           // user data
				0)              // depth
		) {
			retval = EXIT_FAILURE;
		}
	} while (*++argv);

	return retval;
}
