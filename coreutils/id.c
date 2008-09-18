/* vi: set sw=4 ts=4: */
/*
 * Mini id implementation for busybox
 *
 * Copyright (C) 2000 by Randolph Chung <tausq@debian.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 compliant. */
/* Hacked by Tito Ragusa (C) 2004 to handle usernames of whatever length and to
 * be more similar to GNU id.
 * -Z option support: by Yuichi Nakamura <ynakam@hitachisoft.jp>
 * Added -G option Tito Ragusa (C) 2008 for SUSv3.
 */

#include "libbb.h"

#define PRINT_REAL        1
#define NAME_NOT_NUMBER   2
#define JUST_USER         4
#define JUST_GROUP        8
#define JUST_ALL_GROUPS  16
#if ENABLE_SELINUX
#define JUST_CONTEXT     32
#endif

static int printf_full(unsigned id, const char *arg, const char *prefix)
{
	const char *fmt = "%s%u";
	int status = EXIT_FAILURE;

	if (arg) {
		fmt = "%s%u(%s)";
		status = EXIT_SUCCESS;
	}
	printf(fmt, prefix, id, arg);
	return status;
}

#if (defined(__GLIBC__) && !defined(__UCLIBC__))
#define HAVE_getgrouplist 1
#elif ENABLE_USE_BB_PWD_GRP
#define HAVE_getgrouplist 1
#else
#define HAVE_getgrouplist 0
#endif

int id_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int id_main(int argc UNUSED_PARAM, char **argv)
{
	const char *username;
	struct passwd *p;
	uid_t uid;
	gid_t gid;
#if HAVE_getgrouplist
	gid_t *groups;
	int n;
#endif
	unsigned flags;
	short status;
#if ENABLE_SELINUX
	security_context_t scontext;
#endif
	/* Don't allow -n -r -nr -ug -rug -nug -rnug */
	/* Don't allow more than one username */
	opt_complementary = "?1:u--g:g--u:G--u:u--G:g--G:G--g:r?ugG:n?ugG" USE_SELINUX(":u--Z:Z--u:g--Z:Z--g");
	flags = getopt32(argv, "rnugG" USE_SELINUX("Z"));
	username = argv[optind];

	/* This values could be overwritten later */
	uid = geteuid();
	gid = getegid();
	if (flags & PRINT_REAL) {
		uid = getuid();
		gid = getgid();
	}

	if (username) {
#if HAVE_getgrouplist
		int m;
#endif
		p = getpwnam(username);
		/* xuname2uid is needed because it exits on failure */
		uid = xuname2uid(username);
		gid = p->pw_gid; /* in this case PRINT_REAL is the same */

#if HAVE_getgrouplist
		n = 16;
		groups = NULL;
		do {
			m = n;
			groups = xrealloc(groups, sizeof(groups[0]) * m);
			getgrouplist(username, gid, groups, &n); /* GNUism? */
		} while (n > m);
#endif
	} else {
#if HAVE_getgrouplist
		n = getgroups(0, NULL);
		groups = xmalloc(sizeof(groups[0]) * n);
		getgroups(n, groups);
#endif
	}

	if (flags & JUST_ALL_GROUPS) {
#if HAVE_getgrouplist
		while (n--) {
			if (flags & NAME_NOT_NUMBER)
				printf("%s", bb_getgrgid(NULL, 0, *groups++));
			else
				printf("%u", (unsigned) *groups++);
			bb_putchar((n > 0) ? ' ' : '\n');
		}
#endif
		/* exit */
		fflush_stdout_and_exit(EXIT_SUCCESS);
	}

	if (flags & (JUST_GROUP | JUST_USER USE_SELINUX(| JUST_CONTEXT))) {
		/* JUST_GROUP and JUST_USER are mutually exclusive */
		if (flags & NAME_NOT_NUMBER) {
			/* bb_getXXXid(-1) exits on failure, puts cannot segfault */
			puts((flags & JUST_USER) ? bb_getpwuid(NULL, -1, uid) : bb_getgrgid(NULL, -1, gid));
		} else {
			if (flags & JUST_USER) {
				printf("%u\n", (unsigned)uid);
			}
			if (flags & JUST_GROUP) {
				printf("%u\n", (unsigned)gid);
			}
		}

#if ENABLE_SELINUX
		if (flags & JUST_CONTEXT) {
			selinux_or_die();
			if (username) {
				bb_error_msg_and_die("user name can't be passed with -Z");
			}

			if (getcon(&scontext)) {
				bb_error_msg_and_die("can't get process context");
			}
			puts(scontext);
		}
#endif
		/* exit */
		fflush_stdout_and_exit(EXIT_SUCCESS);
	}

	/* Print full info like GNU id */
	/* bb_getpwuid(0) doesn't exit on failure (returns NULL) */
	status = printf_full(uid, bb_getpwuid(NULL, 0, uid), "uid=");
	status |= printf_full(gid, bb_getgrgid(NULL, 0, gid), " gid=");
#if HAVE_getgrouplist
	{
		const char *msg = " groups=";
		while (n--) {
			status |= printf_full(*groups, bb_getgrgid(NULL, 0, *groups), msg);
			msg = ",";
			groups++;
		}
	}
	/* we leak groups vector... */
#endif

#if ENABLE_SELINUX
	if (is_selinux_enabled()) {
		security_context_t mysid;
		getcon(&mysid);
		printf(" context=%s", mysid ? mysid : "unknown");
		if (mysid) /* TODO: maybe freecon(NULL) is harmless? */
			freecon(mysid);
	}
#endif

	bb_putchar('\n');
	fflush_stdout_and_exit(status);
}
