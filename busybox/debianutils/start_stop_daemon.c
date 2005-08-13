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
#include <getopt.h>

#include "busybox.h"
#include "pwd_.h"

static int signal_nr = 15;
static int user_id = -1;
static int quiet = 0;
static char *userspec = NULL;
static char *cmdname = NULL;
static char *execname = NULL;
static char *pidfile = NULL;

struct pid_list {
	struct pid_list *next;
	pid_t pid;
};

static struct pid_list *found = NULL;

static inline void
push(pid_t pid)
{
	struct pid_list *p;

	p = xmalloc(sizeof(*p));
	p->next = found;
	p->pid = pid;
	found = p;
}

static int
pid_is_exec(pid_t pid, const char *name)
{
	char buf[32];
	struct stat sb, exec_stat;

	if (name && stat(name, &exec_stat))
		bb_perror_msg_and_die("stat %s", name);

	sprintf(buf, "/proc/%d/exe", pid);
	if (stat(buf, &sb) != 0)
		return 0;
	return (sb.st_dev == exec_stat.st_dev && sb.st_ino == exec_stat.st_ino);
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
pid_is_cmd(pid_t pid, const char *name)
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
	push(pid);
}


static void
do_pidfile(void)
{
	FILE *f;
	pid_t pid;

	f = fopen(pidfile, "r");
	if (f) {
		if (fscanf(f, "%d", &pid) == 1)
			check(pid);
		fclose(f);
	} else if (errno != ENOENT)
		bb_perror_msg_and_die("open pidfile %s", pidfile);

}

static void
do_procinit(void)
{
	DIR *procdir;
	struct dirent *entry;
	int foundany, pid;

	if (pidfile) {
		do_pidfile();
		return;
	}

	procdir = opendir("/proc");
	if (!procdir)
		bb_perror_msg_and_die ("opendir /proc");

	foundany = 0;
	while ((entry = readdir(procdir)) != NULL) {
		if (sscanf(entry->d_name, "%d", &pid) != 1)
			continue;
		foundany++;
		check(pid);
	}
	closedir(procdir);
	if (!foundany)
		bb_error_msg_and_die ("nothing in /proc - not mounted?");
}


static void
do_stop(void)
{
	char what[1024];
	struct pid_list *p;
	int killed = 0;

	do_procinit();

	if (cmdname)
		strcpy(what, cmdname);
	else if (execname)
		strcpy(what, execname);
	else if (pidfile)
		sprintf(what, "process in pidfile `%.200s'", pidfile);
	else if (userspec)
		sprintf(what, "process(es) owned by `%s'", userspec);
	else
		bb_error_msg_and_die ("internal error, please report");

	if (!found) {
		if (!quiet)
			printf("no %s found; none killed.\n", what);
		return;
	}
	for (p = found; p; p = p->next) {
		if (kill(p->pid, signal_nr) == 0) {
			p->pid = -p->pid;
			killed++;
		} else {
			bb_perror_msg("warning: failed to kill %d", p->pid);
		}
	}
	if (!quiet && killed) {
		printf("stopped %s (pid", what);
		for (p = found; p; p = p->next)
			if(p->pid < 0)
				printf(" %d", -p->pid);
		printf(").\n");
	}
}


static const struct option ssd_long_options[] = {
	{ "stop",				0,		NULL,		'K' },
	{ "start",				0,		NULL,		'S' },
	{ "background",			0,		NULL,		'b' },
	{ "quiet",				0,		NULL,		'q' },
	{ "make-pidfile",		0,		NULL,		'm' },
	{ "startas",			1,		NULL,		'a' },
	{ "name",				1,		NULL,		'n' },
	{ "signal",				1,		NULL,		's' },
	{ "user",				1,		NULL,		'u' },
	{ "exec",				1,		NULL,		'x' },
	{ "pidfile",			1,		NULL,		'p' },
	{ 0,			0,		0,			0 }
};

#define SSD_CTX_STOP		1
#define SSD_CTX_START		2
#define SSD_OPT_BACKGROUND	4
#define SSD_OPT_QUIET		8
#define SSD_OPT_MAKEPID		16

int
start_stop_daemon_main(int argc, char **argv)
{
	unsigned long opt;
	char *signame = NULL;
	char *startas = NULL;

	bb_applet_long_options = ssd_long_options;

	bb_opt_complementaly = "K~S:S~K";
	opt = bb_getopt_ulflags(argc, argv, "KSbqma:n:s:u:x:p:",
			&startas, &cmdname, &signame, &userspec, &execname, &pidfile);

	/* Check one and only one context option was given */
	if ((opt & BB_GETOPT_ERROR) || (opt & (SSD_CTX_STOP | SSD_CTX_START)) == 0) {
		bb_show_usage();
	}

	if (opt & SSD_OPT_QUIET)
		quiet = 1;

	if (signame) {
		signal_nr = bb_xgetlarg(signame, 10, 0, NSIG);
	}

	if (!execname && !pidfile && !userspec && !cmdname)
		bb_error_msg_and_die ("need at least one of -x, -p, -u, or -n");

	if (!startas)
		startas = execname;

	if ((opt & SSD_CTX_START) && !startas)
		bb_error_msg_and_die ("-S needs -x or -a");

	if ((opt & SSD_OPT_MAKEPID) && pidfile == NULL)
		bb_error_msg_and_die ("-m needs -p");

	argc -= optind;
	argv += optind;

	if (userspec && sscanf(userspec, "%d", &user_id) != 1)
		user_id = my_getpwnam(userspec);

	if (opt & SSD_CTX_STOP) {
		do_stop();
		return EXIT_SUCCESS;
	}

	do_procinit();

	if (found) {
		if (!quiet)
			printf("%s already running.\n%d\n", execname ,found->pid);
		return EXIT_SUCCESS;
	}
	*--argv = startas;
	if (opt & SSD_OPT_BACKGROUND) {
		if (daemon(0, 0) == -1)
			bb_perror_msg_and_die ("unable to fork");
		setsid();
	}
	if (opt & SSD_OPT_MAKEPID) {
		/* user wants _us_ to make the pidfile */
		FILE *pidf = fopen(pidfile, "w");
		pid_t pidt = getpid();
		if (pidf == NULL)
			bb_perror_msg_and_die("Unable to write pidfile '%s'", pidfile);
		fprintf(pidf, "%d\n", pidt);
		fclose(pidf);
	}
	execv(startas, argv);
	bb_perror_msg_and_die ("unable to start %s", startas);
}
