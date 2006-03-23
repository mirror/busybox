/*
 * CRONTAB
 *
 * usually setuid root, -c option only works if getuid() == geteuid()
 *
 * Copyright 1994 Matthew Dillon (dillon@apollo.west.oic.com)
 * May be distributed under the GNU General Public License
 *
 * Vladimir Oleynik <dzo@simtreas.ru> (C) 2002 to be used in busybox
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/resource.h>

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
#define PATH_VI         "/bin/vi"   /* location of vi       */
#endif

#include "busybox.h"

static const char  *CDir = CRONTABS;

static void EditFile(const char *user, const char *file);
static int GetReplaceStream(const char *user, const char *file);
static int  ChangeUser(const char *user, short dochdir);

int
crontab_main(int ac, char **av)
{
    enum { NONE, EDIT, LIST, REPLACE, DELETE } option = NONE;
    const struct passwd *pas;
    const char *repFile = NULL;
    int repFd = 0;
    int i;
    char caller[256];           /* user that ran program */
    int   UserId;

    UserId = getuid();
    if ((pas = getpwuid(UserId)) == NULL)
	bb_perror_msg_and_die("getpwuid");

    strncpy(caller, pas->pw_name, sizeof(caller));

    i = 1;
    if (ac > 1) {
	if (av[1][0] == '-' && av[1][1] == 0) {
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

	switch(ptr[-1]) {
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

    if ((pas = getpwuid(UserId)) == NULL)
	bb_perror_msg_and_die("getpwuid");

    /*
     * If there is a replacement file, obtain a secure descriptor to it.
     */

    if (repFile) {
	repFd = GetReplaceStream(caller, repFile);
	if (repFd < 0)
	    bb_error_msg_and_die("unable to read replacement file");
    }

    /*
     * Change directory to our crontab directory
     */

    if (chdir(CDir) < 0)
	bb_perror_msg_and_die("cannot change dir to %s", CDir);

    /*
     * Handle options as appropriate
     */

    switch(option) {
    case LIST:
	{
	    FILE *fi;
	    char buf[1024];

	    if ((fi = fopen(pas->pw_name, "r"))) {
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
	    FILE *fi;
	    int fd;
	    int n;
	    char tmp[128];
	    char buf[1024];

	    snprintf(tmp, sizeof(tmp), TMPDIR "/crontab.%d", getpid());
	    if ((fd = open(tmp, O_RDWR|O_CREAT|O_TRUNC|O_EXCL, 0600)) >= 0) {
		chown(tmp, getuid(), getgid());
		if ((fi = fopen(pas->pw_name, "r"))) {
		    while ((n = fread(buf, 1, sizeof(buf), fi)) > 0)
			write(fd, buf, n);
		}
		EditFile(caller, tmp);
		remove(tmp);
		lseek(fd, 0L, 0);
		repFd = fd;
	    } else {
		bb_error_msg_and_die("unable to create %s", tmp);
	    }

	}
	option = REPLACE;
	/* fall through */
    case REPLACE:
	{
	    char buf[1024];
	    char path[1024];
	    int fd;
	    int n;

	    snprintf(path, sizeof(path), "%s.new", pas->pw_name);
	    if ((fd = open(path, O_CREAT|O_TRUNC|O_APPEND|O_WRONLY, 0600)) >= 0) {
		while ((n = read(repFd, buf, sizeof(buf))) > 0) {
		    write(fd, buf, n);
		}
		close(fd);
		rename(path, pas->pw_name);
	    } else {
		bb_error_msg("unable to create %s/%s", CDir, path);
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
	    bb_error_msg("unable to append to %s/%s", CDir, CRONUPDATE);
	}
    }
    return 0;
}

static int
GetReplaceStream(const char *user, const char *file)
{
    int filedes[2];
    int pid;
    int fd;
    int n;
    char buf[1024];

    if (pipe(filedes) < 0) {
	perror("pipe");
	return(-1);
    }
    if ((pid = fork()) < 0) {
	perror("fork");
	return(-1);
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
	return(filedes[0]);
    }

    /*
     * CHILD
     */

    close(filedes[0]);

    if (ChangeUser(user, 0) < 0)
	exit(0);

    fd = open(file, O_RDONLY);
    if (fd < 0) {
	bb_error_msg("unable to open %s", file);
	exit(0);
    }
    buf[0] = 0;
    write(filedes[1], buf, 1);
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
	write(filedes[1], buf, n);
    }
    exit(0);
}

static void
EditFile(const char *user, const char *file)
{
    int pid;

    if ((pid = fork()) == 0) {
	/*
	 * CHILD - change user and run editor
	 */
	char *ptr;
	char visual[1024];

	if (ChangeUser(user, 1) < 0)
	    exit(0);
	if ((ptr = getenv("VISUAL")) == NULL || strlen(ptr) > 256)
	    ptr = PATH_VI;

	snprintf(visual, sizeof(visual), "%s %s", ptr, file);
	execl(DEFAULT_SHELL, DEFAULT_SHELL, "-c", visual, NULL);
	perror("exec");
	exit(0);
    }
    if (pid < 0) {
	/*
	 * PARENT - failure
	 */
	bb_perror_msg_and_die("fork");
    }
    wait4(pid, NULL, 0, NULL);
}

static int
ChangeUser(const char *user, short dochdir)
{
    struct passwd *pas;

    /*
     * Obtain password entry and change privileges
     */

    if ((pas = getpwnam(user)) == 0) {
	bb_perror_msg_and_die("failed to get uid for %s", user);
	return(-1);
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
	    bb_perror_msg_and_die("chdir failed: %s %s", user, pas->pw_dir);
	    if (chdir(TMPDIR) < 0) {
		bb_perror_msg_and_die("chdir failed: %s %s", user, TMPDIR);
		return(-1);
	    }
	}
    }
    return(pas->pw_uid);
}
