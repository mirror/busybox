/* vi: set sw=4 ts=4: */
/*
 * Mini start-stop-daemon implementation(s) for busybox
 *
 * Written by Marek Michalkiewicz <marekm@i17linuxb.ists.pwr.wroc.pl>,
 * Adapted for busybox David Kimdon <dwhedon@gordian.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include <getopt.h>
#include <sys/resource.h>

static int signal_nr = 15;
static int user_id = -1;
static int quiet;
static char *userspec;
static char *chuid;
static char *cmdname;
static char *execname;
static char *pidfile;

struct pid_list {
	struct pid_list *next;
	pid_t pid;
};

static struct pid_list *found = NULL;

static inline void push(pid_t pid)
{
	struct pid_list *p;

	p = xmalloc(sizeof(*p));
	p->next = found;
	p->pid = pid;
	found = p;
}

static int pid_is_exec(pid_t pid, const char *name)
{
	char buf[sizeof("/proc//exe") + sizeof(int)*3];
	char *execbuf;
	int equal;

	sprintf(buf, "/proc/%d/exe", pid);
	execbuf = xstrdup(name);
	readlink(buf, execbuf, strlen(name)+1);

	equal = ! strcmp(execbuf, name);
	if (ENABLE_FEATURE_CLEAN_UP)
		free(execbuf);
	return equal;
}

static int pid_is_user(int pid, int uid)
{
	struct stat sb;
	char buf[sizeof("/proc/") + sizeof(int)*3];

	sprintf(buf, "/proc/%d", pid);
	if (stat(buf, &sb) != 0)
		return 0;
	return (sb.st_uid == uid);
}

static int pid_is_cmd(pid_t pid, const char *name)
{
	char buf[sizeof("/proc//stat") + sizeof(int)*3];
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


static void check(int pid)
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


static void do_pidfile(void)
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

static void do_procinit(void)
{
	DIR *procdir;
	struct dirent *entry;
	int foundany, pid;

	if (pidfile) {
		do_pidfile();
		return;
	}

	procdir = xopendir("/proc");

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


static int do_stop(void)
{
	char *what;
	struct pid_list *p;
	int killed = 0;

	do_procinit();

	if (cmdname)
		what = xstrdup(cmdname);
	else if (execname)
		what = xstrdup(execname);
	else if (pidfile)
		what = xasprintf("process in pidfile '%s'", pidfile);
	else if (userspec)
		what = xasprintf("process(es) owned by '%s'", userspec);
	else
		bb_error_msg_and_die("internal error, please report");

	if (!found) {
		if (!quiet)
			printf("no %s found; none killed\n", what);
		if (ENABLE_FEATURE_CLEAN_UP)
			free(what);
		return -1;
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
		puts(")");
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		free(what);
	return killed;
}

#if ENABLE_FEATURE_START_STOP_DAEMON_LONG_OPTIONS
static const struct option ssd_long_options[] = {
	{ "stop",               0,      NULL,   'K' },
	{ "start",              0,      NULL,   'S' },
	{ "background",         0,      NULL,   'b' },
	{ "quiet",              0,      NULL,   'q' },
	{ "make-pidfile",       0,      NULL,   'm' },
#if ENABLE_FEATURE_START_STOP_DAEMON_FANCY
	{ "oknodo",             0,      NULL,   'o' },
	{ "verbose",            0,      NULL,   'v' },
	{ "nicelevel",          1,      NULL,   'N' },
#endif
	{ "startas",            1,      NULL,   'a' },
	{ "name",               1,      NULL,   'n' },
	{ "signal",             1,      NULL,   's' },
	{ "user",               1,      NULL,   'u' },
	{ "chuid",              1,      NULL,   'c' },
	{ "exec",               1,      NULL,   'x' },
	{ "pidfile",            1,      NULL,   'p' },
#if ENABLE_FEATURE_START_STOP_DAEMON_FANCY
	{ "retry",              1,      NULL,   'R' },
#endif
	{ 0,                    0,      0,      0 }
};
#endif

#define SSD_CTX_STOP            0x1
#define SSD_CTX_START           0x2
#define SSD_OPT_BACKGROUND      0x4
#define SSD_OPT_QUIET           0x8
#define SSD_OPT_MAKEPID         0x10
#if ENABLE_FEATURE_START_STOP_DAEMON_FANCY
#define SSD_OPT_OKNODO          0x20
#define SSD_OPT_VERBOSE         0x40
#define SSD_OPT_NICELEVEL       0x80
#endif

int start_stop_daemon_main(int argc, char **argv)
{
	unsigned opt;
	char *signame = NULL;
	char *startas = NULL;
#if ENABLE_FEATURE_START_STOP_DAEMON_FANCY
//	char *retry_arg = NULL;
//	int retries = -1;
	char *opt_N;
#endif
#if ENABLE_FEATURE_START_STOP_DAEMON_LONG_OPTIONS
	applet_long_options = ssd_long_options;
#endif

	/* Check required one context option was given */
	opt_complementary = "K:S:?:K--S:S--K:m?p:K?xpun:S?xa";
	opt = getopt32(argc, argv, "KSbqm"
//		USE_FEATURE_START_STOP_DAEMON_FANCY("ovN:R:")
		USE_FEATURE_START_STOP_DAEMON_FANCY("ovN:")
		"a:n:s:u:c:x:p:"
		USE_FEATURE_START_STOP_DAEMON_FANCY(,&opt_N)
//		USE_FEATURE_START_STOP_DAEMON_FANCY(,&retry_arg)
		,&startas, &cmdname, &signame, &userspec, &chuid, &execname, &pidfile);

	quiet = (opt & SSD_OPT_QUIET)
			USE_FEATURE_START_STOP_DAEMON_FANCY(&& !(opt & SSD_OPT_VERBOSE));

	if (signame) {
		signal_nr = get_signum(signame);
		if (signal_nr < 0) bb_show_usage();
	}

	if (!startas)
		startas = execname;

//	USE_FEATURE_START_STOP_DAEMON_FANCY(
//		if (retry_arg)
//			retries = xatoi_u(retry_arg);
//	)
	argc -= optind;
	argv += optind;

	if (userspec && sscanf(userspec, "%d", &user_id) != 1)
		user_id = bb_xgetpwnam(userspec);

	if (opt & SSD_CTX_STOP) {
		int i = do_stop();
		return
			USE_FEATURE_START_STOP_DAEMON_FANCY((opt & SSD_OPT_OKNODO)
				? 0 :) !!(i<=0);
	}

	do_procinit();

	if (found) {
		if (!quiet)
			printf("%s already running\n%d\n", execname, found->pid);
		USE_FEATURE_START_STOP_DAEMON_FANCY(return !(opt & SSD_OPT_OKNODO);)
		SKIP_FEATURE_START_STOP_DAEMON_FANCY(return EXIT_FAILURE;)
	}
	*--argv = startas;
	if (opt & SSD_OPT_BACKGROUND) {
		xdaemon(0, 0);
		setsid();
	}
	if (opt & SSD_OPT_MAKEPID) {
		/* user wants _us_ to make the pidfile */
		FILE *pidf = xfopen(pidfile, "w");

		pid_t pidt = getpid();
		fprintf(pidf, "%d\n", pidt);
		fclose(pidf);
	}
	if (chuid) {
		if (sscanf(chuid, "%d", &user_id) != 1)
			user_id = bb_xgetpwnam(chuid);
		xsetuid(user_id);
	}
#if ENABLE_FEATURE_START_STOP_DAEMON_FANCY
	if (opt & SSD_OPT_NICELEVEL) {
		/* Set process priority */
		int prio = getpriority(PRIO_PROCESS, 0) + xatoi_range(opt_N, INT_MIN/2, INT_MAX/2);
		if (setpriority(PRIO_PROCESS, 0, prio) < 0) {
			bb_perror_msg_and_die("setpriority(%d)", prio);
		}
	}
#endif
	execv(startas, argv);
	bb_perror_msg_and_die("cannot start %s", startas);
}
