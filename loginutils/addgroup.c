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
#include "pwd.h"
#include "grp.h"

#define GROUP_FILE      "/etc/group"
#define SHADOW_FILE		"/etc/gshadow"


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

	etc_group = xfopen(filename, "r");

	/* make sure gr_name isn't taken, make sure gid is kosher */
	desired = g->gr_gid;
	while ((grp = fgetgrent(etc_group))) {
		if ((strcmp(grp->gr_name, g->gr_name)) == 0) {
			error_msg_and_die("%s: group already in use\n", g->gr_name);
		}
		if ((desired) && grp->gr_gid == desired) {
			error_msg_and_die("%d: gid has already been allocated\n",
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
static int addgroup(const char *filename, char *group, gid_t gid)
{
	FILE *etc_group;

#ifdef CONFIG_FEATURE_SHADOWPASSWDS
	FILE *etc_gshadow;
	char *gshadow = SHADOW_FILE;
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
	etc_group = xfopen(filename, "a");

	fprintf(etc_group, entryfmt, group, default_passwd, gr.gr_gid, "");
	fclose(etc_group);


#ifdef CONFIG_FEATURE_SHADOWPASSWDS
	/* add entry to gshadow if necessary */
	if (access(gshadow, F_OK|W_OK) == 0) {
		etc_gshadow = xfopen(gshadow, "a");
		fprintf(etc_gshadow, "%s:!::\n", group);
		fclose(etc_gshadow);
	}
#endif

	/* return 1; */
	return 0;
}

/*
 * addgroup will take a login_name as its first parameter.
 *
 * gid 
 *
 * can be customized via command-line parameters.
 * ________________________________________________________________________ */
int addgroup_main(int argc, char **argv)
{
	int opt;
	char *group;
	gid_t gid = 0;

	/* get remaining args */
	while ((opt = getopt (argc, argv, "g:")) != -1)
		switch (opt) {
			case 'g':
				gid = strtol(optarg, NULL, 10);
				break;
			default:
				show_usage();
				break;
		}

	if (optind >= argc) {
		show_usage();
	} else {
		group = argv[optind];
	}

	if (geteuid() != 0) {
		error_msg_and_die
			("Only root may add a group to the system.");
	}

	/* werk */
	return addgroup(GROUP_FILE, group, gid);
}

/* $Id: addgroup.c,v 1.1 2002/06/04 20:45:05 sandman Exp $ */
