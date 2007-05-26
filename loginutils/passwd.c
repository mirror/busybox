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

	/*memset(salt, 0, sizeof(salt)); - why?*/
	crypt_make_salt(salt, 1); /* des */
	if (algo) { /* MD5 */
		strcpy(salt, "$1$");
		crypt_make_salt(salt + 3, 4);
	}
	ret = xstrdup(pw_encrypt(newp, salt)); /* returns ptr to static */
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


static int update_passwd(const char *filename, const char *username,
			const char *new_pw)
{
	struct stat sb;
	struct flock lock;
	FILE *old_fp;
	FILE *new_fp;
	char *new_name;
	char *last_char;
	unsigned user_len;
	int old_fd;
	int new_fd;
	int i;
	int ret = 1; /* failure */

	logmode = LOGMODE_STDIO;
	/* New passwd file, "/etc/passwd+" for now */
	new_name = xasprintf("%s+", filename);
	last_char = &new_name[strlen(new_name)-1];
	username = xasprintf("%s:", username);
	user_len = strlen(username);

	old_fp = fopen(filename, "r+");
	if (!old_fp)
		goto free_mem;
	old_fd = fileno(old_fp);

	/* Try to create "/etc/passwd+". Wait if it exists. */
	i = 30;
	do {
		// FIXME: on last iteration try w/o O_EXCL but with O_TRUNC?
		new_fd = open(new_name, O_WRONLY|O_CREAT|O_EXCL,0600);
		if (new_fd >= 0) goto created;
		if (errno != EEXIST) break;
		usleep(100000); /* 0.1 sec */
	} while (--i);
	bb_perror_msg("cannot create '%s'", new_name);
	goto close_old_fp;
 created:
	if (!fstat(old_fd, &sb)) {
		fchmod(new_fd, sb.st_mode & 0777); /* ignore errors */
		fchown(new_fd, sb.st_uid, sb.st_gid);
	}
	new_fp = fdopen(new_fd, "w");
	if (!new_fp) {
		close(new_fd);
		goto unlink_new;
	}

	/* Backup file is "/etc/passwd-" */
	last_char[0] = '-';
	/* Delete old one, create new as a hardlink to current */
	i = (unlink(new_name) && errno != ENOENT);
	if (i || link(filename, new_name))
		bb_perror_msg("warning: cannot create backup copy '%s'", new_name);
	last_char[0] = '+';

	/* Lock the password file before updating */
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	if (fcntl(old_fd, F_SETLK, &lock) < 0)
		bb_perror_msg("warning: cannot lock '%s'", filename);
	lock.l_type = F_UNLCK;

	/* Read current password file, write updated one */
	while (1) {
		char *line = xmalloc_fgets(old_fp);
		if (!line) break; /* EOF/error */
		if (strncmp(username, line, user_len) == 0) {
			/* we have a match with "username:"... */
			const char *cp = line + user_len;
			/* now cp -> old passwd, skip it: */
			cp = strchr(cp, ':');
			if (!cp) cp = "";
			/* now cp -> ':' after old passwd or -> "" */
			fprintf(new_fp, "%s%s%s", username, new_pw, cp);
			/* Erase password in memory */
		} else
			fputs(line, new_fp);
		free(line);
	}
	fcntl(old_fd, F_SETLK, &lock);

	/* We do want all of them to execute, thus | instead of || */
	if ((ferror(old_fp) | fflush(new_fp) | fsync(new_fd) | fclose(new_fp))
	 || rename(new_name, filename)
	) {
		/* At least one of those failed */
		goto unlink_new;
	}
	ret = 0; /* whee, success! */

 unlink_new:
	if (ret) unlink(new_name);

 close_old_fp:
	fclose(old_fp);

 free_mem:
	if (ENABLE_FEATURE_CLEAN_UP) free(new_name);
	if (ENABLE_FEATURE_CLEAN_UP) free((char*)username);
	logmode = LOGMODE_BOTH;
	return ret;
}


int passwd_main(int argc, char **argv);
int passwd_main(int argc, char **argv)
{
	enum {
		OPT_algo = 0x1, /* -a - password algorithm */
		OPT_lock = 0x2, /* -l - lock account */
		OPT_unlock = 0x4, /* -u - unlock account */
		OPT_delete = 0x8, /* -d - delete password */
		OPT_lud = 0xe,
		STATE_ALGO_md5 = 0x10,
		/*STATE_ALGO_des = 0x20, not needed yet */
	};
	unsigned opt;
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
	struct spwd *result;
	char buffer[256];
#endif

	logmode = LOGMODE_BOTH;
	openlog(applet_name, LOG_NOWAIT, LOG_AUTH);
	opt = getopt32(argc, argv, "a:lud", &opt_a);
	argc -= optind;
	argv += optind;

	if (strcasecmp(opt_a, "des") != 0) /* -a */
		opt |= STATE_ALGO_md5;
	//else
	//	opt |= STATE_ALGO_des;
	myuid = getuid();
	if ((opt & OPT_lud) && (!argc || myuid))
		bb_show_usage();

	myname = xstrdup(bb_getpwuid(NULL, myuid, -1));
	name = argc ? argv[0] : myname;

	pw = getpwnam(name);
	if (!pw) bb_error_msg_and_die("unknown user %s", name);
	if (myuid && pw->pw_uid != myuid) {
		/* LOGMODE_BOTH */
		bb_error_msg_and_die("%s can't change password for %s", myname, name);
	}

	filename = bb_path_passwd_file;
#if ENABLE_FEATURE_SHADOWPASSWDS
	if (getspnam_r(pw->pw_name, &spw, buffer, sizeof(buffer), &result)) {
		/* LOGMODE_BOTH */
		bb_error_msg("no record of %s in %s, using %s",
				name, bb_path_shadow_file,
				bb_path_passwd_file);
	} else {
		filename = bb_path_shadow_file;
		pw->pw_passwd = spw.sp_pwdp;
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
		newp = xstrdup(&pw->pw_passwd[1]);
	} else if (opt & OPT_delete) {
		newp = xstrdup("");
	}

	rlimit_fsize.rlim_cur = rlimit_fsize.rlim_max = 512L * 30000;
	setrlimit(RLIMIT_FSIZE, &rlimit_fsize);
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	umask(077);
	xsetuid(0);
	if (update_passwd(filename, name, newp) != 0) {
		/* LOGMODE_BOTH */
		bb_error_msg_and_die("cannot update password file %s",
				filename);
	}
	/* LOGMODE_BOTH */
	bb_info_msg("Password for %s changed by %s", name, myname);

	if (ENABLE_FEATURE_CLEAN_UP) free(newp);
skip:
	if (!newp) {
		bb_error_msg_and_die("password for %s is already %slocked",
			name, (opt & OPT_unlock) ? "un" : "");
	}
	if (ENABLE_FEATURE_CLEAN_UP) free(myname);
	return 0;
}
