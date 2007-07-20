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

int update_passwd(const char *filename, const char *username,
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
	int cnt = 0;
	int ret = 1; /* failure */

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
			cnt++;
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
	ret = cnt; /* whee, success! */

 unlink_new:
	if (ret) unlink(new_name);

 close_old_fp:
	fclose(old_fp);

 free_mem:
	if (ENABLE_FEATURE_CLEAN_UP) free(new_name);
	if (ENABLE_FEATURE_CLEAN_UP) free((char*)username);
	return ret;
}
