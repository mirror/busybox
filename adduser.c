/* vi: set sw=4 ts=4: */
/*
 * adduser - add users to /etc/passwd and /etc/shadow
 *
 *
 * Copyright (C) 1999 by Lineo, inc.
 * Written by John Beppu <beppu@lineo.com>
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
#include <shadow.h>
#include <time.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "busybox.h"
#include "pwd_grp/pwd.h"
#include "pwd_grp/grp.h"

#define PASSWD_FILE     "/etc/passwd"
#define SHADOW_FILE		"/etc/gshadow"

#if 0
#  define PASSWD_FILE "passwd"
#  define SHADOW_FILE "shadow"
#endif

/* structs __________________________ */

typedef struct {
	uid_t u;
	gid_t g;
} Id;

/* data _____________________________ */

/* defaults : should this be in an external file? */
static char *default_passwd = "x";
static char *default_gecos = "Embedix User,,,";
static char *default_home_prefix = "/home";
static char *default_shell = "/bin/sh";

/* shadow in use? */
static int shadow_enabled = 0;

/* I was doing this all over the place */
static FILE *fopen_wrapper(const char *filename, const char *mode)
{
	FILE *f;

	f = fopen(filename, mode);
	if (f == NULL) {
		fprintf(stderr, "adduser: %s: %s\n", filename, strerror(errno));
	}
	return f;
}

/* remix */
/* EDR recoded such that the uid may be passed in *p */
static int passwd_study(const char *filename, struct passwd *p)
{
	struct passwd *pw;
	FILE *passwd;

	const int min = 500;
	const int max = 65000;

	passwd = fopen_wrapper(filename, "r");
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

	/* EDR check for an already existing gid */
	while (getgrgid(p->pw_uid) != NULL)
		p->pw_uid++;

	/* EDR also check for an existing group definition */
	if (getgrnam(p->pw_name) != NULL)
		return 3;

	/* EDR bounds check */
	if ((p->pw_uid > max) || (p->pw_uid < min))
		return 2;

	/* EDR create new gid always = uid */
	p->pw_gid = p->pw_uid;

	/* return 1; */
	return 0;
}

static void addgroup_wrapper(const char *login, gid_t gid)
{
	int argc = 4;
	char *argv[] = { "addgroup", "-g", NULL, NULL };
	char group_id[8];
	char group_name[32];

	strncpy(group_name, login, 32);
	argv[3] = group_name;
	sprintf(group_id, "%d", gid);
	argv[2] = group_id;
	addgroup_main(argc, argv);
}

static void passwd_wrapper(const char *login)
{
	char *prog = "passwd";
	execlp(prog, prog, login, NULL);
	error_msg_and_die("Failed to execute 'passwd', you must set the password for '%s' manually\n", login);
}

/*
 * pwd_to_spwd - create entries for new spwd structure
 *
 *	pwd_to_spwd() creates a new (struct spwd) containing the
 *	information in the pointed-to (struct passwd).
 */
#define DAY (24L*3600L)
#define WEEK (7*DAY)
#define SCALE DAY
static struct spwd *pwd_to_spwd(const struct passwd *pw)
{
	static struct spwd sp;

	/*
	 * Nice, easy parts first.  The name and passwd map directly
	 * from the old password structure to the new one.
	 */
	sp.sp_namp = pw->pw_name;
	sp.sp_pwdp = pw->pw_passwd;

	/*
	 * Defaults used if there is no pw_age information.
	 */
	sp.sp_min = 0;
	sp.sp_max = (10000L * DAY) / SCALE;
	sp.sp_lstchg = time((time_t *) 0) / SCALE;

	/*
	 * These fields have no corresponding information in the password
	 * file.  They are set to uninitialized values.
	 */
	sp.sp_warn = -1;
	sp.sp_expire = -1;
	sp.sp_inact = -1;
	sp.sp_flag = -1;

	return &sp;
}

/* putpwent(3) remix */
static int adduser(const char *filename, struct passwd *p)
{
	FILE *passwd;
	int r;
	FILE *shadow;
	struct spwd *sp;

	/* make sure everything is kosher and setup uid && gid */
	passwd = fopen_wrapper(filename, "a");
	if (passwd == NULL) {
		/* return -1; */
		return 1;
	}
	fseek(passwd, 0, SEEK_END);

	/* if (passwd_study(filename, p) == 0) { */
	r = passwd_study(filename, p);
	if (r) {
		if (r == 1)
			error_msg("%s: login already in use\n", p->pw_name);
		else if (r == 2)
			error_msg("illegal uid or no uids left\n");
		else if (r == 3)
			error_msg("group name %s already in use\n", p->pw_name);
		else
			error_msg("generic error.\n");
		/* return -1; */
		return 1;
	}

	/* add to passwd */
	if (putpwent(p, passwd) == -1) {
		/* return -1; */
		return 1;
	}
	fclose(passwd);

	/* add to shadow if necessary */
	if (shadow_enabled) {
		shadow = fopen_wrapper(SHADOW_FILE, "a");
		if (shadow == NULL) {
			/* return -1; */
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

	/* add to group */
	/* addgroup should be responsible for dealing w/ gshadow */
	addgroup_wrapper(p->pw_name, p->pw_gid);

	/* Clear the umask for this process so it doesn't
	 * * screw up the permissions on the mkdir and chown. */
	umask(0);

	/* mkdir */
	if (mkdir(p->pw_dir, 0755)) {
		perror_msg("%s", p->pw_dir);
	}
	/* Set the owner and group so it is owned by the new user. */
	if (chown(p->pw_dir, p->pw_uid, p->pw_gid)) {
		perror_msg("%s", p->pw_dir);
	}
	/* Now fix up the permissions to 2755. Can't do it before now
	 * since chown will clear the setgid bit */
	if (chmod(p->pw_dir, 02755)) {
		perror_msg("%s", p->pw_dir);
	}
	/* interactively set passwd */
	passwd_wrapper(p->pw_name);

	return 0;
}


/* return current uid (root is always uid == 0, right?) */
static uid_t i_am_not_root()
{
	return geteuid();
}

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
	int i;
	char opt;
	char *login;
	char *gecos;
	char *home = NULL;
	char *shell;
	char path[MAXPATHLEN];

	struct passwd pw;

	/* init */
	if (argc < 2) {
		show_usage();
	}
	gecos = default_gecos;
	shell = default_shell;

	/* get args */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			opt = argv[i][1];
			switch (opt) {
			case 'h':
				home = argv[++i];
				break;
			case 'g':
				gecos = argv[++i];
				break;
			case 's':
				shell = argv[++i];
				break;
			default:
				error_msg("invalid option -- %c\n", opt);
				break;
			}
		} else {
			break;
		}
	}

	/* got root? */
	if (i_am_not_root()) {
		error_msg_and_die( "Only root may add a user or group to the system.\n");
	}

	/* get login */
	if (i >= argc) {
		error_msg_and_die( "adduser: no user specified\n");
	}
	login = argv[i];

	/* create string for $HOME if not specified already */
	if (!home) {
		snprintf(path, MAXPATHLEN, "%s/%s", default_home_prefix, login);
		path[MAXPATHLEN - 1] = 0;
		home = path;
	}
	/* is /etc/shadow in use? */
	shadow_enabled = (0 == access(SHADOW_FILE, F_OK));

	/* create a passwd struct */
	pw.pw_name = login;
	pw.pw_passwd = default_passwd;
	pw.pw_uid = 0;
	pw.pw_gid = 0;
	pw.pw_gecos = gecos;
	pw.pw_dir = home;
	pw.pw_shell = shell;

	/* grand finale */
	i = adduser(PASSWD_FILE, &pw);

	return (i);
}

/* $Id: adduser.c,v 1.1 2001/08/21 16:18:59 andersen Exp $ */
