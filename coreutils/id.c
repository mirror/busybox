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
/* Hacked by Tito Ragusa (C) 2004 to handle usernames of whatever length and to
 * be more similar to GNU id. 
 */

#include "busybox.h"
#include "grp_.h"
#include "pwd_.h"
#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/types.h>

#ifdef CONFIG_SELINUX
#include <proc_secure.h>
#include <flask_util.h>
#endif

#define PRINT_REAL        1
#define NAME_NOT_NUMBER   2
#define JUST_USER         4
#define JUST_GROUP        8

void printf_full(unsigned int id, char *arg, char prefix)
{	
	printf("%cid=%u",prefix, id);
	if(arg)
		printf("(%s) ", arg);
}

extern int id_main(int argc, char **argv)
{
	struct passwd *p;
	char *user;
	char *group;
	uid_t uid;
	gid_t gid;
	int flags;
#ifdef CONFIG_SELINUX
	int is_flask_enabled_flag = is_flask_enabled();
#endif

	bb_opt_complementaly = "u~g:g~u";
	flags = bb_getopt_ulflags(argc, argv, "rnug");

	if ((flags & 0x80000000UL)
	 /* Don't allow -n -r -nr */
	|| (flags <= 3 && flags > 0) 
	|| (argc > optind + 1))
		bb_show_usage();
	
	/* This values could be overwritten later */
	uid = geteuid();
	gid = getegid();
	if (flags & PRINT_REAL) {
		uid = getuid();
		gid = getgid();
	}
	
	if(argv[optind])
	{

		p=getpwnam(argv[optind]);
		/* this is needed because it exits on failure */
		uid = my_getpwnam(argv[optind]);
		gid = p->pw_gid;
		/* in this case PRINT_REAL is the same */ 
	}
	
	user=my_getpwuid(NULL, uid, (flags & JUST_USER) ? -1 : 0);

	if(flags & JUST_USER)
	{
		gid=uid;
		group=user;
		goto PRINT; 
	}
	
	group=my_getgrgid(NULL, gid, (flags & JUST_GROUP) ? -1 : 0);

	if(flags & JUST_GROUP) 
	{
PRINT:		
		if(flags & NAME_NOT_NUMBER)
			puts(group);
		else
			printf ("%u\n", gid);
		bb_fflush_stdout_and_exit(EXIT_SUCCESS);
	}
	
	/* Print full info like GNU id */
	printf_full(uid, user, 'u');
	printf_full(gid, group, 'g');
#ifdef CONFIG_SELINUX
	if(is_flask_enabled_flag)
	{
		security_id_t mysid = getsecsid();
		char context[80];
		int len = sizeof(context);
		context[0] = '\0';
		if(security_sid_to_context(mysid, context, &len))
			strcpy(context, "unknown");
		printf("context=%s", context);
	}
#endif
	puts("");
	bb_fflush_stdout_and_exit((user && group) ? EXIT_SUCCESS : EXIT_FAILURE);
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/

