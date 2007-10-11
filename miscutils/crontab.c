/* vi: set sw=4 ts=4: */
/*
 * CRONTAB
 *
 * usually setuid root, -c option only works if getuid() == geteuid()
 *
 * Copyright 1994 Matthew Dillon (dillon@apollo.west.oic.com)
 * Vladimir Oleynik <dzo@simtreas.ru> (C) 2002
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "libbb.h"

#ifndef CRONTABS
#define CRONTABS        "/var/spool/cron/crontabs"
#endif
#ifndef TMPDIR
#define TMPDIR          "/var/spool/cron"
#endif
#ifndef CRONUPDATE
#define CRONUPDATE      "cron.update"
#endif
#ifndef PATH_VI
#define PATH_VI         "/bin/vi"   /* location of vi */
#endif

static const char *CDir = CRONTABS;

static void EditFile(const char *user, const char *file);
static int GetReplaceStream(const char *user, const char *file);
static int ChangeUser(const char *user, short dochdir);

int crontab_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int crontab_main(int ac, char **av)
{
	enum { NONE, EDIT, LIST, REPLACE, DELETE } option = NONE;
	const struct passwd *pas;
	const char *repFile = NULL;
	int repFd = 0;
	int i;
	char caller[256];           /* user that ran program */
	char buf[1024];
	int UserId;

	UserId = getuid();
	pas = getpwuid(UserId);
	if (pas == NULL)
		bb_perror_msg_and_die("getpwuid");

	safe_strncpy(caller, pas->pw_name, sizeof(caller));

	i = 1;
	if (ac > 1) {
		if (LONE_DASH(av[1])) {
			option = REPLACE;
			++i;
		} else if (av[1][0] != '-') {
			option = REPLACE;
			++i;
			repFile = av[1];
		}
	}

	for (; i < ac; ++i) {
		char *ptr = av[i];

		if (*ptr != '-')
			break;
		ptr += 2;

		switch (ptr[-1]) {
		case 'l':
			if (ptr[-1] == 'l')
				option = LIST;
			/* fall through */
		case 'e':
			if (ptr[-1] == 'e')
				option = EDIT;
			/* fall through */
		case 'd':
			if (ptr[-1] == 'd')
				option = DELETE;
			/* fall through */
		case 'u':
			if (i + 1 < ac && av[i+1][0] != '-') {
				++i;
				if (getuid() == geteuid()) {
					pas = getpwnam(av[i]);
					if (pas) {
						UserId = pas->pw_uid;
					} else {
						bb_error_msg_and_die("user %s unknown", av[i]);
					}
				} else {
					bb_error_msg_and_die("only the superuser may specify a user");
				}
			}
			break;
		case 'c':
			if (getuid() == geteuid()) {
				CDir = (*ptr) ? ptr : av[++i];
			} else {
				bb_error_msg_and_die("-c option: superuser only");
			}
			break;
		default:
			i = ac;
			break;
		}
	}
	if (i != ac || option == NONE)
		bb_show_usage();

	/*
	 * Get password entry
	 */

	pas = getpwuid(UserId);
	if (pas == NULL)
		bb_perror_msg_and_die("getpwuid");

	/*
	 * If there is a replacement file, obtain a secure descriptor to it.
	 */

	if (repFile) {
		repFd = GetReplaceStream(caller, repFile);
		if (repFd < 0)
			bb_error_msg_and_die("cannot read replacement file");
	}

	/*
	 * Change directory to our crontab directory
	 */

	xchdir(CDir);

	/*
	 * Handle options as appropriate
	 */

	switch (option) {
	case LIST:
		{
			FILE *fi;

			fi = fopen(pas->pw_name, "r");
			if (fi) {
				while (fgets(buf, sizeof(buf), fi) != NULL)
					fputs(buf, stdout);
				fclose(fi);
			} else {
				bb_error_msg("no crontab for %s", pas->pw_name);
			}
		}
		break;
	case EDIT:
		{
/* FIXME: messy code here! we have file copying helpers for this! */
			FILE *fi;
			int fd;
			int n;
			char tmp[128];

			snprintf(tmp, sizeof(tmp), TMPDIR "/crontab.%d", getpid());
			fd = xopen3(tmp, O_RDWR|O_CREAT|O_TRUNC|O_EXCL, 0600);
/* race, use fchown */
			chown(tmp, getuid(), getgid());
			fi = fopen(pas->pw_name, "r");
			if (fi) {
				while ((n = fread(buf, 1, sizeof(buf), fi)) > 0)
					full_write(fd, buf, n);
			}
			EditFile(caller, tmp);
			remove(tmp);
			lseek(fd, 0L, SEEK_SET);
			repFd = fd;
		}
		option = REPLACE;
		/* fall through */
	case REPLACE:
		{
/* same here */
			char path[1024];
			int fd;
			int n;

			snprintf(path, sizeof(path), "%s.new", pas->pw_name);
			fd = open(path, O_CREAT|O_TRUNC|O_APPEND|O_WRONLY, 0600);
			if (fd >= 0) {
				while ((n = read(repFd, buf, sizeof(buf))) > 0) {
					full_write(fd, buf, n);
				}
				close(fd);
				rename(path, pas->pw_name);
			} else {
				bb_error_msg("cannot create %s/%s", CDir, path);
			}
			close(repFd);
		}
		break;
	case DELETE:
		remove(pas->pw_name);
		break;
	case NONE:
	default:
		break;
	}

	/*
	 *  Bump notification file.  Handle window where crond picks file up
	 *  before we can write our entry out.
	 */

	if (option == REPLACE || option == DELETE) {
		FILE *fo;
		struct stat st;

		while ((fo = fopen(CRONUPDATE, "a"))) {
			fprintf(fo, "%s\n", pas->pw_name);
			fflush(fo);
			if (fstat(fileno(fo), &st) != 0 || st.st_nlink != 0) {
				fclose(fo);
				break;
			}
			fclose(fo);
			/* loop */
		}
		if (fo == NULL) {
			bb_error_msg("cannot append to %s/%s", CDir, CRONUPDATE);
		}
	}
	return 0;
}

static int GetReplaceStream(const char *user, const char *file)
{
	int filedes[2];
	int pid;
	int fd;
	int n;
	char buf[1024];

	if (pipe(filedes) < 0) {
		perror("pipe");
		return -1;
	}
	pid = fork();
	if (pid < 0) {
		perror("fork");
		return -1;
	}
	if (pid > 0) {
		/*
		 * PARENT
		 */

		close(filedes[1]);
		if (read(filedes[0], buf, 1) != 1) {
			close(filedes[0]);
			filedes[0] = -1;
		}
		return filedes[0];
	}

	/*
	 * CHILD
	 */

	close(filedes[0]);

	if (ChangeUser(user, 0) < 0)
		exit(0);

	xfunc_error_retval = 0;
	fd = xopen(file, O_RDONLY);
	buf[0] = 0;
	write(filedes[1], buf, 1);
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		write(filedes[1], buf, n);
	}
	exit(0);
}

static void EditFile(const char *user, const char *file)
{
	int pid = fork();

	if (pid == 0) {
		/*
		 * CHILD - change user and run editor
		 */
		const char *ptr;

		if (ChangeUser(user, 1) < 0)
			exit(0);
		ptr = getenv("VISUAL");
		if (ptr == NULL)
			ptr = getenv("EDITOR");
		if (ptr == NULL)
			ptr = PATH_VI;

		ptr = xasprintf("%s %s", ptr, file);
		execl(DEFAULT_SHELL, DEFAULT_SHELL, "-c", ptr, NULL);
		bb_perror_msg_and_die("exec");
	}
	if (pid < 0) {
		/*
		 * PARENT - failure
		 */
		bb_perror_msg_and_die("fork");
	}
	wait4(pid, NULL, 0, NULL);
}

static int ChangeUser(const char *user, short dochdir)
{
	struct passwd *pas;

	/*
	 * Obtain password entry and change privileges
	 */

	pas = getpwnam(user);
	if (pas == NULL) {
		bb_perror_msg_and_die("cannot get uid for %s", user);
	}
	setenv("USER", pas->pw_name, 1);
	setenv("HOME", pas->pw_dir, 1);
	setenv("SHELL", DEFAULT_SHELL, 1);

	/*
	 * Change running state to the user in question
	 */
	change_identity(pas);

	if (dochdir) {
		if (chdir(pas->pw_dir) < 0) {
			bb_perror_msg("chdir(%s) by %s failed", pas->pw_dir, user);
			xchdir(TMPDIR);
		}
	}
	return pas->pw_uid;
}
