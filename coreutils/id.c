/* vi: set sw=4 ts=4: */
/*
 * Mini id implementation for busybox
 *
 * Copyright (C) 2000 by Randolph Chung <tausq@debian.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
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
#include <selinux/selinux.h>          /* for is_selinux_enabled() */
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

int id_main(int argc, char **argv)
{
	struct passwd *p;
	uid_t uid;
	gid_t gid;
	unsigned long flags;
	short status;

	/* Don't allow -n -r -nr -ug -rug -nug -rnug */
	/* Don't allow more than one username */
	bb_opt_complementally = "?1:?:u--g:g--u:r?ug:n?ug";
	flags = bb_getopt_ulflags(argc, argv, "rnug");

	/* This values could be overwritten later */
	uid = geteuid();
	gid = getegid();
	if (flags & PRINT_REAL) {
		uid = getuid();
		gid = getgid();
	}

	if(argv[optind]) {
		p=getpwnam(argv[optind]);
		/* bb_xgetpwnam is needed because it exits on failure */
		uid = bb_xgetpwnam(argv[optind]);
		gid = p->pw_gid;
		/* in this case PRINT_REAL is the same */
	}

	if(flags & (JUST_GROUP | JUST_USER)) {
		/* JUST_GROUP and JUST_USER are mutually exclusive */
		if(flags & NAME_NOT_NUMBER) {
			/* bb_getpwuid and bb_getgrgid exit on failure so puts cannot segfault */
			puts((flags & JUST_USER) ? bb_getpwuid(NULL, uid, -1 ) : bb_getgrgid(NULL, gid, -1 ));
		} else {
			bb_printf("%u\n",(flags & JUST_USER) ? uid : gid);
		}
		/* exit */
		bb_fflush_stdout_and_exit(EXIT_SUCCESS);
	}

	/* Print full info like GNU id */
	/* bb_getpwuid doesn't exit on failure here */
	status=printf_full(uid, bb_getpwuid(NULL, uid, 0), 'u');
	putchar(' ');
	/* bb_getgrgid doesn't exit on failure here */
	status|=printf_full(gid, bb_getgrgid(NULL, gid, 0), 'g');

#ifdef CONFIG_SELINUX
	if ( is_selinux_enabled() ) {
			security_context_t mysid;
			char context[80];
			int len = sizeof(context);

			getcon(&mysid);
			context[0] = '\0';
			if (mysid) {
					len = strlen(mysid)+1;
					safe_strncpy(context, mysid, len);
					freecon(mysid);
			}else{
					safe_strncpy(context, "unknown",8);
			}
		bb_printf(" context=%s", context);
	}
#endif

	putchar('\n');
	bb_fflush_stdout_and_exit(status);
}
