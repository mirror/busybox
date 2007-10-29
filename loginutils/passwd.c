/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include <syslog.h>


static void nuke_str(char *str)
{
	if (str) memset(str, 0, strlen(str));
}

static char* new_password(const struct passwd *pw, uid_t myuid, int algo)
{
	char salt[sizeof("$N$XXXXXXXX")]; /* "$N$XXXXXXXX" or "XX" */
	char *orig = (char*)"";
	char *newp = NULL;
	char *cipher = NULL;
	char *cp = NULL;
	char *ret = NULL; /* failure so far */

	if (myuid && pw->pw_passwd[0]) {
		orig = bb_askpass(0, "Old password:"); /* returns ptr to static */
		if (!orig)
			goto err_ret;
		cipher = pw_encrypt(orig, pw->pw_passwd); /* returns ptr to static */
		if (strcmp(cipher, pw->pw_passwd) != 0) {
			syslog(LOG_WARNING, "incorrect password for '%s'",
				pw->pw_name);
			bb_do_delay(FAIL_DELAY);
			puts("Incorrect password");
			goto err_ret;
		}
	}
	orig = xstrdup(orig); /* or else bb_askpass() will destroy it */
	newp = bb_askpass(0, "New password:"); /* returns ptr to static */
	if (!newp)
		goto err_ret;
	newp = xstrdup(newp); /* we are going to bb_askpass() again, so save it */
	if (ENABLE_FEATURE_PASSWD_WEAK_CHECK
	 && obscure(orig, newp, pw) && myuid)
		goto err_ret; /* non-root is not allowed to have weak passwd */

	cp = bb_askpass(0, "Retype password:");
	if (!cp)
		goto err_ret;
	if (strcmp(cp, newp)) {
		puts("Passwords don't match");
		goto err_ret;
	}

	crypt_make_salt(salt, 1, 0); /* des */
	if (algo) { /* MD5 */
		strcpy(salt, "$1$");
		crypt_make_salt(salt + 3, 4, 0);
	}
	/* pw_encrypt returns ptr to static */
	ret = xstrdup(pw_encrypt(newp, salt));
	/* whee, success! */

 err_ret:
	nuke_str(orig);
	if (ENABLE_FEATURE_CLEAN_UP) free(orig);
	nuke_str(newp);
	if (ENABLE_FEATURE_CLEAN_UP) free(newp);
	nuke_str(cipher);
	nuke_str(cp);
	return ret;
}

int passwd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int passwd_main(int argc, char **argv)
{
	enum {
		OPT_algo = 0x1, /* -a - password algorithm */
		OPT_lock = 0x2, /* -l - lock account */
		OPT_unlock = 0x4, /* -u - unlock account */
		OPT_delete = 0x8, /* -d - delete password */
		OPT_lud = 0xe,
		STATE_ALGO_md5 = 0x10,
		//STATE_ALGO_des = 0x20, not needed yet
	};
	unsigned opt;
	int rc;
	const char *opt_a = "";
	const char *filename;
	char *myname;
	char *name;
	char *newp;
	struct passwd *pw;
	uid_t myuid;
	struct rlimit rlimit_fsize;
	char c;
#if ENABLE_FEATURE_SHADOWPASSWDS
	/* Using _r function to avoid pulling in static buffers */
	struct spwd spw;
	char buffer[256];
#endif

	logmode = LOGMODE_BOTH;
	openlog(applet_name, LOG_NOWAIT, LOG_AUTH);
	opt = getopt32(argv, "a:lud", &opt_a);
	//argc -= optind;
	argv += optind;

	if (strcasecmp(opt_a, "des") != 0) /* -a */
		opt |= STATE_ALGO_md5;
	//else
	//	opt |= STATE_ALGO_des;
	myuid = getuid();
	/* -l, -u, -d require root priv and username argument */
	if ((opt & OPT_lud) && (myuid || !argv[0]))
		bb_show_usage();

	/* Will complain and die if username not found */
	myname = xstrdup(bb_getpwuid(NULL, -1, myuid));
	name = argv[0] ? argv[0] : myname;

	pw = getpwnam(name);
	if (!pw) bb_error_msg_and_die("unknown user %s", name);
	if (myuid && pw->pw_uid != myuid) {
		/* LOGMODE_BOTH */
		bb_error_msg_and_die("%s can't change password for %s", myname, name);
	}

#if ENABLE_FEATURE_SHADOWPASSWDS
	{
		/* getspnam_r may return 0 yet set result to NULL.
		 * At least glibc 2.4 does this. Be extra paranoid here. */
		struct spwd *result = NULL;
		if (getspnam_r(pw->pw_name, &spw, buffer, sizeof(buffer), &result)
		 || !result || strcmp(result->sp_namp, pw->pw_name) != 0) {
			/* LOGMODE_BOTH */
			bb_error_msg("no record of %s in %s, using %s",
					name, bb_path_shadow_file,
					bb_path_passwd_file);
		} else {
			pw->pw_passwd = result->sp_pwdp;
		}
	}
#endif

	/* Decide what the new password will be */
	newp = NULL;
	c = pw->pw_passwd[0] - '!';
	if (!(opt & OPT_lud)) {
		if (myuid && !c) { /* passwd starts with '!' */
			/* LOGMODE_BOTH */
			bb_error_msg_and_die("cannot change "
					"locked password for %s", name);
		}
		printf("Changing password for %s\n", name);
		newp = new_password(pw, myuid, opt & STATE_ALGO_md5);
		if (!newp) {
			logmode = LOGMODE_STDIO;
			bb_error_msg_and_die("password for %s is unchanged", name);
		}
	} else if (opt & OPT_lock) {
		if (!c) goto skip; /* passwd starts with '!' */
		newp = xasprintf("!%s", pw->pw_passwd);
	} else if (opt & OPT_unlock) {
		if (c) goto skip; /* not '!' */
		/* pw->pw_passwd points to static storage,
		 * strdup'ing to avoid nasty surprizes */
		newp = xstrdup(&pw->pw_passwd[1]);
	} else if (opt & OPT_delete) {
		//newp = xstrdup("");
		newp = (char*)"";
	}

	rlimit_fsize.rlim_cur = rlimit_fsize.rlim_max = 512L * 30000;
	setrlimit(RLIMIT_FSIZE, &rlimit_fsize);
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	umask(077);
	xsetuid(0);

#if ENABLE_FEATURE_SHADOWPASSWDS
	filename = bb_path_shadow_file;
	rc = update_passwd(bb_path_shadow_file, name, newp);
	if (rc == 0) /* no lines updated, no errors detected */
#endif
	{
		filename = bb_path_passwd_file;
		rc = update_passwd(bb_path_passwd_file, name, newp);
	}
	/* LOGMODE_BOTH */
	if (rc < 0)
		bb_error_msg_and_die("cannot update password file %s",
				filename);
	bb_info_msg("Password for %s changed by %s", name, myname);

	//if (ENABLE_FEATURE_CLEAN_UP) free(newp);
 skip:
	if (!newp) {
		bb_error_msg_and_die("password for %s is already %slocked",
			name, (opt & OPT_unlock) ? "un" : "");
	}
	if (ENABLE_FEATURE_CLEAN_UP) free(myname);
	return 0;
}
