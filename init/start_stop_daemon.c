/* vi: set sw=4 ts=4: */
/*
 * Mini start-stop-daemon implementation(s) for busybox
 *
 * Written by Marek Michalkiewicz <marekm@i17linuxb.ists.pwr.wroc.pl>,
 * public domain.
 * Adapted for busybox David Kimdon <dwhedon@gordian.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include "pwd.h"

#include "busybox.h"

static int start = 0;
static int stop = 0;
static int signal_nr = 15;
static int user_id = -1;
static const char *userspec = NULL;
static const char *cmdname = NULL;
static char *execname = NULL;
static char *startas = NULL;
static const char *progname = "";

struct pid_list {
	struct pid_list *next;
	int pid;
};

static struct pid_list *found = NULL;
static struct pid_list *killed = NULL;

static void
push(struct pid_list **list, int pid)
{
	struct pid_list *p;

	p = xmalloc(sizeof(*p));
	p->next = *list;
	p->pid = pid;
	*list = p;
}


static void
parse_options(int argc, char * const *argv)
{
	
	int c;

	for (;;) {
	    c = getopt (argc, argv, "a:n:s:u:x:KS");
		if (c == EOF)
			break;
		switch (c) {
		case 'K':
			stop = 1;
			break;
		case 'S':
			start = 1;
			break;
		case 'a':
			startas = optarg;
			break;
		case 'n':
			cmdname = optarg;
			break;
		case 's':
			if (sscanf(optarg, "%d", &signal_nr) != 1)
				error_msg_and_die ("-s takes a numeric argument");
			break;
		case 'u':
			userspec = optarg;
			break;
		case 'x':
			execname = optarg;
			break;
		default:
			show_usage();
			exit(1);
		}
	}

	if (start == stop)
		error_msg_and_die ("need one of -S or -K");

	if (!execname && !userspec)
		error_msg_and_die ("need at least one of -x or -u");

	if (!startas)
		startas = execname;

	if (start && !startas)
		error_msg_and_die ("-S needs -x or -a");
}


static int
pid_is_exec(int pid, const char *exec)
{
	char buf[PATH_MAX];
	FILE *fp;

	sprintf(buf, "/proc/%d/cmdline", pid);
	fp = fopen(buf, "r");
	if (fp && fgets (buf, sizeof (buf), fp) ) {
	    if (strncmp (buf, exec, strlen(exec)) == 0)
		return 1;
	}
	return 0;
}


static int
pid_is_user(int pid, int uid)
{
	struct stat sb;
	char buf[32];

	sprintf(buf, "/proc/%d", pid);
	if (stat(buf, &sb) != 0)
		return 0;
	return (sb.st_uid == uid);
}


static int
pid_is_cmd(int pid, const char *name)
{
	char buf[32];
	FILE *f;
	int c;

	sprintf(buf, "/proc/%d/stat", pid);
	f = fopen(buf, "r");
	if (!f)
		return 0;
	while ((c = getc(f)) != EOF && c != '(')
		;
	if (c != '(') {
		fclose(f);
		return 0;
	}
	/* this hopefully handles command names containing ')' */
	while ((c = getc(f)) != EOF && c == *name)
		name++;
	fclose(f);
	return (c == ')' && *name == '\0');
}


static void
check(int pid)
{
	if (execname && !pid_is_exec(pid, execname)) {
		return;
	}
	if (userspec && !pid_is_user(pid, user_id)) {
		return;
	}
	if (cmdname && !pid_is_cmd(pid, cmdname)) {
		return;
	}
	push(&found, pid);
}



static void
do_procfs(void)
{
	DIR *procdir;
	struct dirent *entry;
	int foundany, pid;

	procdir = opendir("/proc");
	if (!procdir)
		perror_msg_and_die ("opendir /proc");

	foundany = 0;
	while ((entry = readdir(procdir)) != NULL) {
		if (sscanf(entry->d_name, "%d", &pid) != 1)
			continue;
		foundany++;
		check(pid);
	}
	closedir(procdir);
	if (!foundany)
		error_msg_and_die ("nothing in /proc - not mounted?");
}


static void
do_stop(void)
{
	char what[1024];
	struct pid_list *p;

	if (cmdname)
		strcpy(what, cmdname);
	else if (execname)
		strcpy(what, execname);
	else if (userspec)
		sprintf(what, "process(es) owned by `%s'", userspec);
	else
		error_msg_and_die ("internal error, please report");

	if (!found) {
		printf("no %s found; none killed.\n", what);
		exit(0);
	}
	for (p = found; p; p = p->next) {
		if (kill(p->pid, signal_nr) == 0)
			push(&killed, p->pid);
		else
			printf("%s: warning: failed to kill %d: %s\n",
			       progname, p->pid, strerror(errno));
	}
	if (killed) {
		printf("stopped %s (pid", what);
		for (p = killed; p; p = p->next)
			printf(" %d", p->pid);
		printf(").\n");
	}
}


int
start_stop_daemon_main(int argc, char **argv)
{
	progname = argv[0];

	parse_options(argc, argv);
	argc -= optind;
	argv += optind;

	if (userspec && sscanf(userspec, "%d", &user_id) != 1) {
		struct passwd *pw;

		pw = getpwnam(userspec);
		if (!pw)
			error_msg_and_die ("user `%s' not found\n", userspec);

		user_id = pw->pw_uid;
	}

	do_procfs();

	if (stop) {
		do_stop();
		exit(0);
	}

	if (found) {
		printf("%s already running.\n", execname);
		printf("%d\n",found->pid);
		exit(0);
	}
	*--argv = startas;
	execv(startas, argv);
	perror_msg_and_die ("unable to start %s", startas);
}

