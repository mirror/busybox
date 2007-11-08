/* vi: set sw=4 ts=4: */
/*
 * update_passwd
 *
 * update_passwd is a common function for passwd and chpasswd applets;
 * it is responsible for updating password file (i.e. /etc/passwd or
 * /etc/shadow) for a given user and password.
 *
 * Moved from loginutils/passwd.c by Alexander Shishkin <virtuoso@slind.org>
 */

#include "libbb.h"

#if ENABLE_SELINUX
static void check_selinux_update_passwd(const char *username)
{
	security_context_t context;
	char *seuser;

	if (getuid() != (uid_t)0 || is_selinux_enabled() == 0)
		return;		/* No need to check */

	if (getprevcon_raw(&context) < 0)
		bb_perror_msg_and_die("getprevcon failed");
	seuser = strtok(context, ":");
	if (!seuser)
		bb_error_msg_and_die("invalid context '%s'", context);
	if (strcmp(seuser, username) != 0) {
		if (checkPasswdAccess(PASSWD__PASSWD) != 0)
			bb_error_msg_and_die("SELinux: access denied");
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		freecon(context);
}
#else
#define check_selinux_update_passwd(username) ((void)0)
#endif

int update_passwd(const char *filename, const char *username,
			const char *new_pw)
{
	struct stat sb;
	struct flock lock;
	FILE *old_fp;
	FILE *new_fp;
	char *fnamesfx;
	char *sfx_char;
	unsigned user_len;
	int old_fd;
	int new_fd;
	int i;
	int cnt = 0;
	int ret = -1; /* failure */

	filename = xmalloc_follow_symlinks(filename);
	if (filename == NULL)
		return -1;

	check_selinux_update_passwd(username);

	/* New passwd file, "/etc/passwd+" for now */
	fnamesfx = xasprintf("%s+", filename);
	sfx_char = &fnamesfx[strlen(fnamesfx)-1];
	username = xasprintf("%s:", username);
	user_len = strlen(username);

	old_fp = fopen(filename, "r+");
	if (!old_fp)
		goto free_mem;
	old_fd = fileno(old_fp);

	selinux_preserve_fcontext(old_fd);

	/* Try to create "/etc/passwd+". Wait if it exists. */
	i = 30;
	do {
		// FIXME: on last iteration try w/o O_EXCL but with O_TRUNC?
		new_fd = open(fnamesfx, O_WRONLY|O_CREAT|O_EXCL, 0600);
		if (new_fd >= 0) goto created;
		if (errno != EEXIST) break;
		usleep(100000); /* 0.1 sec */
	} while (--i);
	bb_perror_msg("cannot create '%s'", fnamesfx);
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
	*sfx_char = '-';
	/* Delete old backup */
	i = (unlink(fnamesfx) && errno != ENOENT);
	/* Create backup as a hardlink to current */
	if (i || link(filename, fnamesfx))
		bb_perror_msg("warning: cannot create backup copy '%s'", fnamesfx);
	*sfx_char = '+';

	/* Lock the password file before updating */
	lock.l_type = F_WRLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	if (fcntl(old_fd, F_SETLK, &lock) < 0)
		bb_perror_msg("warning: cannot lock '%s'", filename);
	lock.l_type = F_UNLCK;

	/* Read current password file, write updated /etc/passwd+ */
	while (1) {
		char *line = xmalloc_fgets(old_fp);
		if (!line) break; /* EOF/error */
		if (strncmp(username, line, user_len) == 0) {
			/* we have a match with "username:"... */
			const char *cp = line + user_len;
			/* now cp -> old passwd, skip it: */
			cp = strchrnul(cp, ':');
			/* now cp -> ':' after old passwd or -> "" */
			fprintf(new_fp, "%s%s%s", username, new_pw, cp);
			cnt++;
		} else
			fputs(line, new_fp);
		free(line);
	}
	fcntl(old_fd, F_SETLK, &lock);

	/* We do want all of them to execute, thus | instead of || */
	if ((ferror(old_fp) | fflush(new_fp) | fsync(new_fd) | fclose(new_fp))
	 || rename(fnamesfx, filename)
	) {
		/* At least one of those failed */
		goto unlink_new;
	}
	ret = cnt; /* whee, success! */

 unlink_new:
	if (ret < 0) unlink(fnamesfx);

 close_old_fp:
	fclose(old_fp);

 free_mem:
	free(fnamesfx);
	free((char *)filename);
	free((char *)username);
	return ret;
}
