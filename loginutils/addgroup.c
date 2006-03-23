/* vi: set sw=4 ts=4: */
/*
 * addgroup - add users to /etc/passwd and /etc/shadow
 *
 * Copyright (C) 1999 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
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

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "busybox.h"
#include "pwd_.h"
#include "grp_.h"


/* structs __________________________ */

/* data _____________________________ */

/* defaults : should this be in an external file? */
static const char default_passwd[] = "x";


/* make sure gr_name isn't taken, make sure gid is kosher
 * return 1 on failure */
static int group_study(const char *filename, struct group *g)
{
	FILE *etc_group;
	gid_t desired;

	struct group *grp;
	const int max = 65000;

	etc_group = bb_xfopen(filename, "r");

	/* make sure gr_name isn't taken, make sure gid is kosher */
	desired = g->gr_gid;
	while ((grp = fgetgrent(etc_group))) {
		if ((strcmp(grp->gr_name, g->gr_name)) == 0) {
			bb_error_msg_and_die("%s: group already in use\n", g->gr_name);
		}
		if ((desired) && grp->gr_gid == desired) {
			bb_error_msg_and_die("%d: gid has already been allocated\n",
							  desired);
		}
		if ((grp->gr_gid > g->gr_gid) && (grp->gr_gid < max)) {
			g->gr_gid = grp->gr_gid;
		}
	}
	fclose(etc_group);

	/* gid */
	if (desired) {
		g->gr_gid = desired;
	} else {
		g->gr_gid++;
	}
	/* return 1; */
	return 0;
}

/* append a new user to the passwd file */
static int addgroup(const char *filename, char *group, gid_t gid, const char *user)
{
	FILE *etc_group;

#ifdef CONFIG_FEATURE_SHADOWPASSWDS
	FILE *etc_gshadow;
#endif

	struct group gr;

	/* group:passwd:gid:userlist */
	static const char entryfmt[] = "%s:%s:%d:%s\n";

	/* make sure gid and group haven't already been allocated */
	gr.gr_gid = gid;
	gr.gr_name = group;
	if (group_study(filename, &gr))
		return 1;

	/* add entry to group */
	etc_group = bb_xfopen(filename, "a");

	fprintf(etc_group, entryfmt, group, default_passwd, gr.gr_gid, user);
	fclose(etc_group);


#ifdef CONFIG_FEATURE_SHADOWPASSWDS
	/* add entry to gshadow if necessary */
	if (access(bb_path_gshadow_file, F_OK|W_OK) == 0) {
		etc_gshadow = bb_xfopen(bb_path_gshadow_file, "a");
		fprintf(etc_gshadow, "%s:!::\n", group);
		fclose(etc_gshadow);
	}
#endif

	/* return 1; */
	return 0;
}

#ifndef CONFIG_ADDUSER
static inline void if_i_am_not_root(void)
{
	if (geteuid()) {
		bb_error_msg_and_die( "Only root may add a user or group to the system.");
	}
}
#else
extern void if_i_am_not_root(void);
#endif

/*
 * addgroup will take a login_name as its first parameter.
 *
 * gid
 *
 * can be customized via command-line parameters.
 * ________________________________________________________________________ */
int addgroup_main(int argc, char **argv)
{
	char *group;
	char *user;
	gid_t gid = 0;

	/* get remaining args */
	if(bb_getopt_ulflags(argc, argv, "g:", &group)) {
		gid = bb_xgetlarg(group, 10, 0, LONG_MAX);
	}

	if (optind < argc) {
		group = argv[optind];
		optind++;
	} else {
		bb_show_usage();
	}

	if (optind < argc) {
		user = argv[optind];
	} else {
		user = "";
	}
	
	if_i_am_not_root();

	/* werk */
	return addgroup(bb_path_group_file, group, gid, user);
}
