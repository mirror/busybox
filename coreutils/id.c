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
 * -Z option support: by Yuichi Nakamura <ynakam@hitachisoft.jp>
 */

#include "busybox.h"
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#define PRINT_REAL        1
#define NAME_NOT_NUMBER   2
#define JUST_USER         4
#define JUST_GROUP        8
#if ENABLE_SELINUX
#define JUST_CONTEXT    16
#endif

static short printf_full(unsigned int id, const char *arg, const char prefix)
{
	const char *fmt = "%cid=%u";
	short status = EXIT_FAILURE;

	if (arg) {
		fmt = "%cid=%u(%s)";
		status = EXIT_SUCCESS;
	}
	printf(fmt, prefix, id, arg);
	return status;
}

int id_main(int argc, char **argv);
int id_main(int argc, char **argv)
{
	struct passwd *p;
	uid_t uid;
	gid_t gid;
	unsigned long flags;
	short status;
#if ENABLE_SELINUX
	security_context_t scontext;
#endif
	/* Don't allow -n -r -nr -ug -rug -nug -rnug */
	/* Don't allow more than one username */
	opt_complementary = "?1:?:u--g:g--u:r?ug:n?ug" USE_SELINUX(":u--Z:Z--u:g--Z:Z--g");
	flags = getopt32(argc, argv, "rnug" USE_SELINUX("Z"));

	/* This values could be overwritten later */
	uid = geteuid();
	gid = getegid();
	if (flags & PRINT_REAL) {
		uid = getuid();
		gid = getgid();
	}

	if (argv[optind]) {
		p = getpwnam(argv[optind]);
		/* xuname2uid is needed because it exits on failure */
		uid = xuname2uid(argv[optind]);
		gid = p->pw_gid;
		/* in this case PRINT_REAL is the same */
	}

	if (flags & (JUST_GROUP | JUST_USER USE_SELINUX(| JUST_CONTEXT))) {
		/* JUST_GROUP and JUST_USER are mutually exclusive */
		if (flags & NAME_NOT_NUMBER) {
			/* bb_getpwuid and bb_getgrgid exit on failure so puts cannot segfault */
			puts((flags & JUST_USER) ? bb_getpwuid(NULL, uid, -1 ) : bb_getgrgid(NULL, gid, -1 ));
		} else {
			if (flags & JUST_USER) {
				printf("%u\n", uid);
			}
			if (flags & JUST_GROUP) {
				printf("%u\n", gid);
			}
		}

#if ENABLE_SELINUX
		if (flags & JUST_CONTEXT) {
			selinux_or_die();
			if (argc - optind == 1) {
				bb_error_msg_and_die("user name can't be passed with -Z");
			}

			if (getcon(&scontext)) {
				bb_error_msg_and_die("can't get process context");
			}
			printf("%s\n", scontext);
		}
#endif
		/* exit */
		fflush_stdout_and_exit(EXIT_SUCCESS);
	}

	/* Print full info like GNU id */
	/* bb_getpwuid doesn't exit on failure here */
	status = printf_full(uid, bb_getpwuid(NULL, uid, 0), 'u');
	putchar(' ');
	/* bb_getgrgid doesn't exit on failure here */
	status |= printf_full(gid, bb_getgrgid(NULL, gid, 0), 'g');

#if ENABLE_SELINUX
	if (is_selinux_enabled()) {
		security_context_t mysid;
		const char *context;

		context = "unknown";
		getcon(&mysid);
		if (mysid) {
			context = alloca(strlen(mysid) + 1);
			strcpy((char*)context, mysid);
			freecon(mysid);
		}
		printf(" context=%s", context);
	}
#endif

	putchar('\n');
	fflush_stdout_and_exit(status);
}
