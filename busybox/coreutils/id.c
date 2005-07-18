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
#include "pwd_.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef CONFIG_SELINUX
#include <proc_secure.h>
#include <flask_util.h>
#endif

#define PRINT_REAL        1
#define NAME_NOT_NUMBER   2
#define JUST_USER         4
#define JUST_GROUP        8

static short printf_full(unsigned int id, const char *arg, const char prefix)
{	
	const char *fmt = "%cid=%u";
	short status=EXIT_FAILURE;
	
	if(arg) {
		fmt = "%cid=%u(%s)";
		status=EXIT_SUCCESS;
	}
	bb_printf(fmt, prefix, id, arg);
	return status;
}

extern int id_main(int argc, char **argv)
{
	struct passwd *p;
	uid_t uid;
	gid_t gid;
	unsigned long flags;
	short status;
#ifdef CONFIG_SELINUX
	int is_flask_enabled_flag = is_flask_enabled();
#endif

	bb_opt_complementaly = "u~g:g~u";
	flags = bb_getopt_ulflags(argc, argv, "rnug");

	if ((flags & BB_GETOPT_ERROR)
	/* Don't allow -n -r -nr */
	|| (flags <= 3 && flags > 0) 
	/* Don't allow more than one username */
	|| (argc > optind + 1))
		bb_show_usage();
	
	/* This values could be overwritten later */
	uid = geteuid();
	gid = getegid();
	if (flags & PRINT_REAL) {
		uid = getuid();
		gid = getgid();
	}
	
	if(argv[optind]) {
		p=getpwnam(argv[optind]);
		/* my_getpwnam is needed because it exits on failure */
		uid = my_getpwnam(argv[optind]);
		gid = p->pw_gid;
		/* in this case PRINT_REAL is the same */ 
	}

	if(flags & (JUST_GROUP | JUST_USER)) {
		/* JUST_GROUP and JUST_USER are mutually exclusive */
		if(flags & NAME_NOT_NUMBER) {
			/* my_getpwuid and my_getgrgid exit on failure so puts cannot segfault */
			puts((flags & JUST_USER) ? my_getpwuid(NULL, uid, -1 ) : my_getgrgid(NULL, gid, -1 ));
		} else {
			bb_printf("%u\n",(flags & JUST_USER) ? uid : gid);
		}
		/* exit */ 
		bb_fflush_stdout_and_exit(EXIT_SUCCESS);
	}

	/* Print full info like GNU id */
	/* my_getpwuid doesn't exit on failure here */
	status=printf_full(uid, my_getpwuid(NULL, uid, 0), 'u');
	putchar(' ');
	/* my_getgrgid doesn't exit on failure here */
	status|=printf_full(gid, my_getgrgid(NULL, gid, 0), 'g');
#ifdef CONFIG_SELINUX
	if(is_flask_enabled_flag) {
		security_id_t mysid = getsecsid();
		char context[80];
		int len = sizeof(context);
		context[0] = '\0';
		if(security_sid_to_context(mysid, context, &len))
			strcpy(context, "unknown");
		bb_printf(" context=%s", context);
	}
#endif
	putchar('\n');
	bb_fflush_stdout_and_exit(status);
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/

