/* vi: set sw=4 ts=4: */
/*
 * adduser - add users to /etc/passwd and /etc/shadow
 *
 * Copyright (C) 1999 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "busybox.h"

#define OPT_DONT_SET_PASS  (1 << 4)
#define OPT_DONT_MAKE_HOME (1 << 6)


/* remix */
/* EDR recoded such that the uid may be passed in *p */
static int passwd_study(const char *filename, struct passwd *p)
{
	enum { min = 500, max = 65000 };
	FILE *passwd;
	/* We are using reentrant fgetpwent_r() in order to avoid
	 * pulling in static buffers from libc (think static build here) */
	char buffer[256];
	struct passwd pw;
	struct passwd *result;

	passwd = xfopen(filename, "r");

	/* EDR if uid is out of bounds, set to min */
	if ((p->pw_uid > max) || (p->pw_uid < min))
		p->pw_uid = min;

	/* stuff to do:
	 * make sure login isn't taken;
	 * find free uid and gid;
	 */
	while (!fgetpwent_r(passwd, &pw, buffer, sizeof(buffer), &result)) {
		if (strcmp(pw.pw_name, p->pw_name) == 0) {
			/* return 0; */
			return 1;
		}
		if ((pw.pw_uid >= p->pw_uid) && (pw.pw_uid < max)
			&& (pw.pw_uid >= min)) {
			p->pw_uid = pw.pw_uid + 1;
		}
	}

	if (p->pw_gid == 0) {
		/* EDR check for an already existing gid */
		while (getgrgid(p->pw_uid) != NULL)
			p->pw_uid++;

		/* EDR also check for an existing group definition */
		if (getgrnam(p->pw_name) != NULL)
			return 3;

		/* EDR create new gid always = uid */
		p->pw_gid = p->pw_uid;
	}

	/* EDR bounds check */
	if ((p->pw_uid > max) || (p->pw_uid < min))
		return 2;

	/* return 1; */
	return 0;
}

static void addgroup_wrapper(struct passwd *p)
{
	char *cmd;

	cmd = xasprintf("addgroup -g %d \"%s\"", p->pw_gid, p->pw_name);
	system(cmd);
	free(cmd);
}

static void passwd_wrapper(const char *login) ATTRIBUTE_NORETURN;

static void passwd_wrapper(const char *login)
{
	static const char prog[] = "passwd";
	BB_EXECLP(prog, prog, login, NULL);
	bb_error_msg_and_die("failed to execute '%s', you must set the password for '%s' manually", prog, login);
}

/* putpwent(3) remix */
static int adduser(struct passwd *p)
{
	FILE *file;
	int addgroup = !p->pw_gid;

	/* make sure everything is kosher and setup uid && gid */
	file = xfopen(bb_path_passwd_file, "a");
	fseek(file, 0, SEEK_END);

	switch (passwd_study(bb_path_passwd_file, p)) {
		case 1:
			bb_error_msg_and_die("%s: login already in use", p->pw_name);
		case 2:
			bb_error_msg_and_die("illegal uid or no uids left");
		case 3:
			bb_error_msg_and_die("%s: group name already in use", p->pw_name);
	}

	/* add to passwd */
	if (putpwent(p, file) == -1) {
		bb_perror_nomsg_and_die();
	}
	fclose(file);

#if ENABLE_FEATURE_SHADOWPASSWDS
	/* add to shadow if necessary */
	file = xfopen(bb_path_shadow_file, "a");
	fseek(file, 0, SEEK_END);
	fprintf(file, "%s:!:%ld:%d:%d:%d:::\n",
			p->pw_name,             /* username */
			time(NULL) / 86400,     /* sp->sp_lstchg */
			0,                      /* sp->sp_min */
			99999,                  /* sp->sp_max */
			7);                     /* sp->sp_warn */
	fclose(file);
#endif

	/* add to group */
	/* addgroup should be responsible for dealing w/ gshadow */
	/* if using a pre-existing group, don't create one */
	if (addgroup) addgroup_wrapper(p);

	/* Clear the umask for this process so it doesn't
	 * * screw up the permissions on the mkdir and chown. */
	umask(0);
	if (!(option_mask32 & OPT_DONT_MAKE_HOME)) {
		/* Set the owner and group so it is owned by the new user,
		   then fix up the permissions to 2755. Can't do it before
		   since chown will clear the setgid bit */
		if (mkdir(p->pw_dir, 0755)
		|| chown(p->pw_dir, p->pw_uid, p->pw_gid)
		|| chmod(p->pw_dir, 02755)) {
			bb_perror_msg("%s", p->pw_dir);
		}
	}

	if (!(option_mask32 & OPT_DONT_SET_PASS)) {
		/* interactively set passwd */
		passwd_wrapper(p->pw_name);
	}

	return 0;
}

/*
 * adduser will take a login_name as its first parameter.
 *
 * home
 * shell
 * gecos
 *
 * can be customized via command-line parameters.
 */
int adduser_main(int argc, char **argv);
int adduser_main(int argc, char **argv)
{
	struct passwd pw;
	const char *usegroup = NULL;

	/* got root? */
	if (geteuid()) {
		bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);
	}

	pw.pw_gecos = (char *)"Linux User,,,";
	pw.pw_shell = (char *)DEFAULT_SHELL;
	pw.pw_dir = NULL;

	/* check for min, max and missing args and exit on error */
	opt_complementary = "-1:?1:?";
	getopt32(argc, argv, "h:g:s:G:DSH", &pw.pw_dir, &pw.pw_gecos, &pw.pw_shell, &usegroup);

	/* create string for $HOME if not specified already */
	if (!pw.pw_dir) {
		snprintf(bb_common_bufsiz1, BUFSIZ, "/home/%s", argv[optind]);
		pw.pw_dir = bb_common_bufsiz1;
	}

	/* create a passwd struct */
	pw.pw_name = argv[optind];
	pw.pw_passwd = (char *)"x";
	pw.pw_uid = 0;
	pw.pw_gid = usegroup ? xgroup2gid(usegroup) : 0; /* exits on failure */

	/* grand finale */
	return adduser(&pw);
}
