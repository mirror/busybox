/* vi: set sw=4 ts=4: */
/*
 * adduser - add users to /etc/passwd and /etc/shadow
 *
 * Copyright (C) 1999 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "libbb.h"

#define OPT_DONT_SET_PASS  (1 << 4)
#define OPT_SYSTEM_ACCOUNT (1 << 5)
#define OPT_DONT_MAKE_HOME (1 << 6)


/* remix */
/* recoded such that the uid may be passed in *p */
static void passwd_study(struct passwd *p)
{
	int max;

	if (getpwnam(p->pw_name))
		bb_error_msg_and_die("login '%s' is in use", p->pw_name);

	if (option_mask32 & OPT_SYSTEM_ACCOUNT) {
		p->pw_uid = 0;
		max = 999;
	} else {
		p->pw_uid = 1000;
		max = 64999;
	}

	/* check for a free uid (and maybe gid) */
	while (getpwuid(p->pw_uid) || (!p->pw_gid && getgrgid(p->pw_uid)))
		p->pw_uid++;

	if (!p->pw_gid) {
		/* new gid = uid */
		p->pw_gid = p->pw_uid;
		if (getgrnam(p->pw_name))
			bb_error_msg_and_die("group name '%s' is in use", p->pw_name);
	}

	if (p->pw_uid > max)
		bb_error_msg_and_die("no free uids left");
}

static void addgroup_wrapper(struct passwd *p)
{
	char *cmd;

	cmd = xasprintf("addgroup -g %u '%s'", (unsigned)p->pw_gid, p->pw_name);
	system(cmd);
	free(cmd);
}

static void passwd_wrapper(const char *login) ATTRIBUTE_NORETURN;

static void passwd_wrapper(const char *login)
{
	static const char prog[] ALIGN1 = "passwd";

	BB_EXECLP(prog, prog, login, NULL);
	bb_error_msg_and_die("cannot execute %s, you must set password manually", prog);
}

/*
 * adduser will take a login_name as its first parameter.
 * home, shell, gecos:
 * can be customized via command-line parameters.
 */
int adduser_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int adduser_main(int argc, char **argv)
{
	struct passwd pw;
	const char *usegroup = NULL;
	FILE *file;

	/* got root? */
	if (geteuid()) {
		bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);
	}

	pw.pw_gecos = (char *)"Linux User,,,";
	pw.pw_shell = (char *)DEFAULT_SHELL;
	pw.pw_dir = NULL;

	/* exactly one non-option arg */
	opt_complementary = "=1";
	getopt32(argv, "h:g:s:G:DSH", &pw.pw_dir, &pw.pw_gecos, &pw.pw_shell, &usegroup);
	argv += optind;

	/* fill in the passwd struct */
	pw.pw_name = argv[0];
	if (!pw.pw_dir) {
		/* create string for $HOME if not specified already */
		pw.pw_dir = xasprintf("/home/%s", argv[0]);
	}
	pw.pw_passwd = (char *)"x";
	pw.pw_gid = usegroup ? xgroup2gid(usegroup) : 0; /* exits on failure */

	/* make sure everything is kosher and setup uid && maybe gid */
	passwd_study(&pw);

	/* add to passwd */
	file = xfopen(bb_path_passwd_file, "a");
	//fseek(file, 0, SEEK_END); /* paranoia, "a" should ensure that anyway */
	if (putpwent(&pw, file) != 0) {
		bb_perror_nomsg_and_die();
	}
	/* do fclose even if !ENABLE_FEATURE_CLEAN_UP.
	 * We will exec passwd, files must be flushed & closed before that! */
	fclose(file);

#if ENABLE_FEATURE_SHADOWPASSWDS
	/* add to shadow if necessary */
	file = fopen_or_warn(bb_path_shadow_file, "a");
	if (file) {
		//fseek(file, 0, SEEK_END);
		fprintf(file, "%s:!:%u:0:99999:7:::\n",
				pw.pw_name,             /* username */
				(unsigned)(time(NULL) / 86400) /* sp->sp_lstchg */
				/*0,*/                  /* sp->sp_min */
				/*99999,*/              /* sp->sp_max */
				/*7*/                   /* sp->sp_warn */
		);
		fclose(file);
	}
#endif

	/* add to group */
	/* addgroup should be responsible for dealing w/ gshadow */
	/* if using a pre-existing group, don't create one */
	if (!usegroup)
		addgroup_wrapper(&pw);

	/* Clear the umask for this process so it doesn't
	 * screw up the permissions on the mkdir and chown. */
	umask(0);
	if (!(option_mask32 & OPT_DONT_MAKE_HOME)) {
		/* Set the owner and group so it is owned by the new user,
		   then fix up the permissions to 2755. Can't do it before
		   since chown will clear the setgid bit */
		if (mkdir(pw.pw_dir, 0755)
		 || chown(pw.pw_dir, pw.pw_uid, pw.pw_gid)
		 || chmod(pw.pw_dir, 02755) /* set setgid bit on homedir */
		) {
			bb_simple_perror_msg(pw.pw_dir);
		}
	}

	if (!(option_mask32 & OPT_DONT_SET_PASS)) {
		/* interactively set passwd */
		passwd_wrapper(pw.pw_name);
	}

	return 0;
}
