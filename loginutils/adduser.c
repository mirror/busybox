/* vi: set sw=4 ts=4: */
/*
 * adduser - add users to /etc/passwd and /etc/shadow
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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "busybox.h"



/* structs __________________________ */

typedef struct {
	uid_t u;
	gid_t g;
} Id;

/* data _____________________________ */

/* defaults : should this be in an external file? */
static const char default_passwd[] = "x";
static const char default_gecos[] = "Linux User,,,";
static const char default_home_prefix[] = "/home";

#ifdef CONFIG_FEATURE_SHADOWPASSWDS
/* shadow in use? */
static int shadow_enabled = 0;
#endif

/* remix */
/* EDR recoded such that the uid may be passed in *p */
static int passwd_study(const char *filename, struct passwd *p)
{
	struct passwd *pw;
	FILE *passwd;

	const int min = 500;
	const int max = 65000;

	passwd = bb_wfopen(filename, "r");
	if (!passwd)
		return 4;

	/* EDR if uid is out of bounds, set to min */
	if ((p->pw_uid > max) || (p->pw_uid < min))
		p->pw_uid = min;

	/* stuff to do:
	 * make sure login isn't taken;
	 * find free uid and gid;
	 */
	while ((pw = fgetpwent(passwd))) {
		if (strcmp(pw->pw_name, p->pw_name) == 0) {
			/* return 0; */
			return 1;
		}
		if ((pw->pw_uid >= p->pw_uid) && (pw->pw_uid < max)
			&& (pw->pw_uid >= min)) {
			p->pw_uid = pw->pw_uid + 1;
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

static void addgroup_wrapper(const char *login, gid_t gid)
{
	char *cmd;

	bb_xasprintf(&cmd, "addgroup -g %d %s", gid, login);
	system(cmd);
	free(cmd);
}

static void passwd_wrapper(const char *login) __attribute__ ((noreturn));

static void passwd_wrapper(const char *login)
{
	static const char prog[] = "passwd";
	execlp(prog, prog, login, NULL);
	bb_error_msg_and_die("Failed to execute '%s', you must set the password for '%s' manually", prog, login);
}

/* putpwent(3) remix */
static int adduser(const char *filename, struct passwd *p, int makehome, int setpass)
{
	FILE *passwd;
	int r;
#ifdef CONFIG_FEATURE_SHADOWPASSWDS
	FILE *shadow;
	struct spwd *sp;
#endif
	int new_group = 1;

	/* if using a pre-existing group, don't create one */
	if (p->pw_gid != 0)
		new_group = 0;

	/* make sure everything is kosher and setup uid && gid */
	passwd = bb_wfopen(filename, "a");
	if (passwd == NULL) {
		return 1;
	}
	fseek(passwd, 0, SEEK_END);

	/* if (passwd_study(filename, p) == 0) { */
	r = passwd_study(filename, p);
	if (r) {
		if (r == 1)
			bb_error_msg("%s: login already in use", p->pw_name);
		else if (r == 2)
			bb_error_msg("illegal uid or no uids left");
		else if (r == 3)
			bb_error_msg("group name %s already in use", p->pw_name);
		else
			bb_error_msg("generic error.");
		return 1;
	}

	/* add to passwd */
	if (putpwent(p, passwd) == -1) {
		return 1;
	}
	fclose(passwd);

#ifdef CONFIG_FEATURE_SHADOWPASSWDS
	/* add to shadow if necessary */
	if (shadow_enabled) {
		shadow = bb_wfopen(bb_path_shadow_file, "a");
		if (shadow == NULL) {
			return 1;
		}
		fseek(shadow, 0, SEEK_END);
		sp = pwd_to_spwd(p);
		sp->sp_max = 99999;		/* debianish */
		sp->sp_warn = 7;
		fprintf(shadow, "%s:!:%ld:%ld:%ld:%ld:::\n",
				sp->sp_namp, sp->sp_lstchg, sp->sp_min, sp->sp_max,
				sp->sp_warn);
		fclose(shadow);
	}
#endif

	if (new_group) {
		/* add to group */
		/* addgroup should be responsible for dealing w/ gshadow */
		addgroup_wrapper(p->pw_name, p->pw_gid);
	}

	/* Clear the umask for this process so it doesn't
	 * * screw up the permissions on the mkdir and chown. */
	umask(0);

	if (makehome) {
		/* mkdir */
		if (mkdir(p->pw_dir, 0755)) {
			bb_perror_msg("%s", p->pw_dir);
		}
		/* Set the owner and group so it is owned by the new user. */
		if (chown(p->pw_dir, p->pw_uid, p->pw_gid)) {
			bb_perror_msg("%s", p->pw_dir);
		}
		/* Now fix up the permissions to 2755. Can't do it before now
		 * since chown will clear the setgid bit */
		if (chmod(p->pw_dir, 02755)) {
			bb_perror_msg("%s", p->pw_dir);
		}
	}

	if (setpass) {
		/* interactively set passwd */
		passwd_wrapper(p->pw_name);
	}

	return 0;
}


/* return current uid (root is always uid == 0, right?) */
#ifndef CONFIG_ADDGROUP
static inline void if_i_am_not_root(void)
#else
void if_i_am_not_root(void)
#endif
{
	if (geteuid()) {
		bb_error_msg_and_die( "Only root may add a user or group to the system.");
	}
}

#define SETPASS				(1 << 4)
#define MAKEHOME			(1 << 6)

/*
 * adduser will take a login_name as its first parameter.
 *
 * home
 * shell
 * gecos
 *
 * can be customized via command-line parameters.
 * ________________________________________________________________________ */
int adduser_main(int argc, char **argv)
{
	struct passwd pw;
	const char *login;
	const char *gecos = default_gecos;
	const char *home = NULL;
	const char *shell = DEFAULT_SHELL;
 	const char *usegroup = NULL;
	int flags;
	int setpass = 1;
	int makehome = 1;

	/* init */
	if (argc < 2) {
		bb_show_usage();
	}
	/* get args */
	flags = bb_getopt_ulflags(argc, argv, "h:g:s:G:DSH", &home, &gecos, &shell, &usegroup);

	if (flags & SETPASS) {
		setpass = 0;
	}
	if (flags & MAKEHOME) {
		makehome = 0;
	}

	/* got root? */
	if_i_am_not_root();

	/* get login */
	if (optind >= argc) {
		bb_error_msg_and_die( "no user specified");
	}
	login = argv[optind];

	/* create string for $HOME if not specified already */
	if (!home) {
		home = concat_path_file(default_home_prefix, login);
	}
#ifdef CONFIG_FEATURE_SHADOWPASSWDS
	/* is /etc/shadow in use? */
	shadow_enabled = (0 == access(bb_path_shadow_file, F_OK));
#endif

	/* create a passwd struct */
	pw.pw_name = (char *)login;
	pw.pw_passwd = (char *)default_passwd;
	pw.pw_uid = 0;
	pw.pw_gid = 0;
	pw.pw_gecos = (char *)gecos;
	pw.pw_dir = (char *)home;
	pw.pw_shell = (char *)shell;

	if (usegroup) {
		/* Add user to a group that already exists */
		pw.pw_gid = my_getgrnam(usegroup);
		/* exits on error */	
	}

	/* grand finale */
	return adduser(bb_path_passwd_file, &pw, makehome, setpass);
}
