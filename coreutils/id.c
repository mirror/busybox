/* vi: set sw=4 ts=4: */
/*
 * Mini id implementation for busybox
 *
 * Copyright (C) 2000 by Randolph Chung <tausq@debian.org>
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

/* BB_AUDIT SUSv3 _NOT_ compliant -- option -G is not currently supported. */

#include "busybox.h"
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/types.h>
#ifdef CONFIG_SELINUX
#include <proc_secure.h>
#include <flask_util.h>
#endif

#define JUST_USER         1
#define JUST_GROUP        2
#define PRINT_REAL        4
#define NAME_NOT_NUMBER   8

extern int id_main(int argc, char **argv)
{
	char user[9], group[9];
	long pwnam, grnam;
	int uid, gid;
	int flags;
#ifdef CONFIG_SELINUX
	int is_flask_enabled_flag = is_flask_enabled();
#endif
	
	flags = bb_getopt_ulflags(argc, argv, "ugrn");

	if (((flags & (JUST_USER | JUST_GROUP)) == (JUST_USER | JUST_GROUP))
		|| (argc > optind + 1)
	) {
		bb_show_usage();
	}

	if (argv[optind] == NULL) {
		if (flags & PRINT_REAL) {
			uid = getuid();
			gid = getgid();
		} else {
			uid = geteuid();
			gid = getegid();
		}
		my_getpwuid(user, uid);
	} else {
		safe_strncpy(user, argv[optind], sizeof(user));
	    gid = my_getpwnamegid(user);
	}
	my_getgrgid(group, gid);

	pwnam=my_getpwnam(user);
	grnam=my_getgrnam(group);

	if (flags & (JUST_GROUP | JUST_USER)) {
		char *s = group;
		if (flags & JUST_USER) {
			s = user;
			grnam = pwnam;
		}
		if (flags & NAME_NOT_NUMBER) {
			puts(s);
		} else {
			printf("%ld\n", grnam);
		}
	} else {
#ifdef CONFIG_SELINUX
		printf("uid=%ld(%s) gid=%ld(%s)", pwnam, user, grnam, group);
		if(is_flask_enabled_flag)
		{
			security_id_t mysid = getsecsid();
			char context[80];
			int len = sizeof(context);
			context[0] = '\0';
			if(security_sid_to_context(mysid, context, &len))
				strcpy(context, "unknown");
			printf(" context=%s\n", context);
		}
		else
			printf("\n");
#else
		printf("uid=%ld(%s) gid=%ld(%s)\n", pwnam, user, grnam, group);
#endif

	}

	bb_fflush_stdout_and_exit(0);
}
