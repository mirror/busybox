/* vi: set sw=4 ts=4: */
/*
 * Mini start-stop-daemon implementation(s) for busybox
 *
 * Written by Marek Michalkiewicz <marekm@i17linuxb.ists.pwr.wroc.pl>,
 * Adapted for busybox David Kimdon <dwhedon@gordian.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* NB: we have a problem here with /proc/NN/exe usage, similar to
 * one fixed in killall/pidof */

#include <getopt.h>
#include <sys/resource.h>

/* Override ENABLE_FEATURE_PIDFILE */
#define WANT_PIDFILE 1
#include "libbb.h"

static int signal_nr = 15;
static int user_id = -1;
static char *userspec;
static char *cmdname;
static char *execname;
static char *pidfile;
static smallint quiet;

struct pid_list {
	struct pid_list *next;
	pid_t pid;
};

static struct pid_list *found;

static int pid_is_exec(pid_t pid, const char *name)
{
	char buf[sizeof("/proc//exe") + sizeof(int)*3];
	char *execbuf;
	int n;

	sprintf(buf, "/proc/%u/exe", pid);
	n = strlen(name) + 1;
	execbuf = xzalloc(n + 1);
	readlink(buf, execbuf, n);

	/* if readlink fails, execbuf still contains "" */
	n = strcmp(execbuf, name);
	if (ENABLE_FEATURE_CLEAN_UP)
		free(execbuf);
	return !n; /* nonzero (true) if execbuf == name */
}

static int pid_is_user(int pid, int uid)
{
	struct stat sb;
	char buf[sizeof("/proc/") + sizeof(int)*3];

	sprintf(buf, "/proc/%u", pid);
	if (stat(buf, &sb) != 0)
		return 0;
	return (sb.st_uid == uid);
}

static int pid_is_cmd(pid_t pid, const char *name)
{
	char fname[sizeof("/proc//stat") + sizeof(int)*3];
	char *buf;
	int r = 0;

	sprintf(fname, "/proc/%u/stat", pid);
	buf = xmalloc_open_read_close(fname, NULL);
	if (buf) {
		char *p = strchr(buf, '(');
		if (p) {
			char *pe = strrchr(++p, ')');
			if (pe) {
				*pe = '\0';
				r = !strcmp(p, name);
			}
		}
		free(buf);
	}
	return r;
}

static void check(int pid)
{
	struct pid_list *p;

	if (execname && !pid_is_exec(pid, execname)) {
		return;
	}
	if (userspec && !pid_is_user(pid, user_id)) {
		return;
	}
	if (cmdname && !pid_is_cmd(pid, cmdname)) {
		return;
	}
	p = xmalloc(sizeof(*p));
	p->next = found;
	p->pid = pid;
	found = p;
}

static void do_pidfile(void)
{
	FILE *f;
	unsigned pid;

	f = fopen(pidfile, "r");
	if (f) {
		if (fscanf(f, "%u", &pid) == 1)
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
		pid = bb_strtou(entry->d_name, NULL, 10);
		if (errno)
			continue;
		foundany++;
		check(pid);
	}
	closedir(procdir);
	if (!foundany)
		bb_error_msg_and_die("nothing in /proc - not mounted?");
}

static int do_stop(void)
{
	char *what;
	struct pid_list *p;
	int killed = 0;

	do_procinit();

	if (cmdname) {
		if (ENABLE_FEATURE_CLEAN_UP) what = xstrdup(cmdname);
		if (!ENABLE_FEATURE_CLEAN_UP) what = cmdname;
	} else if (execname) {
		if (ENABLE_FEATURE_CLEAN_UP) what = xstrdup(execname);
		if (!ENABLE_FEATURE_CLEAN_UP) what = execname;
	} else if (pidfile)
		what = xasprintf("process in pidfile '%s'", pidfile);
	else if (userspec)
		what = xasprintf("process(es) owned by '%s'", userspec);
	else
		bb_error_msg_and_die("internal error, please report");

	if (!found) {
		if (!quiet)
			printf("no %s found; none killed\n", what);
		killed = -1;
		goto ret;
	}
	for (p = found; p; p = p->next) {
		if (kill(p->pid, signal_nr) == 0) {
			p->pid = - p->pid;
			killed++;
		} else {
			bb_perror_msg("warning: killing process %u", p->pid);
		}
	}
	if (!quiet && killed) {
		printf("stopped %s (pid", what);
		for (p = found; p; p = p->next)
			if (p->pid < 0)
				printf(" %u", - p->pid);
		puts(")");
	}
 ret:
	if (ENABLE_FEATURE_CLEAN_UP)
		free(what);
	return killed;
}

#if ENABLE_FEATURE_START_STOP_DAEMON_LONG_OPTIONS
static const char start_stop_daemon_longopts[] ALIGN1 =
	"stop\0"         No_argument       "K"
	"start\0"        No_argument       "S"
	"background\0"   No_argument       "b"
	"quiet\0"        No_argument       "q"
	"make-pidfile\0" No_argument       "m"
#if ENABLE_FEATURE_START_STOP_DAEMON_FANCY
	"oknodo\0"       No_argument       "o"
	"verbose\0"      No_argument       "v"
	"nicelevel\0"    Required_argument "N"
#endif
	"startas\0"      Required_argument "a"
	"name\0"         Required_argument "n"
	"signal\0"       Required_argument "s"
	"user\0"         Required_argument "u"
	"chuid\0"        Required_argument "c"
	"exec\0"         Required_argument "x"
	"pidfile\0"      Required_argument "p"
#if ENABLE_FEATURE_START_STOP_DAEMON_FANCY
	"retry\0"        Required_argument "R"
#endif
	;
#endif

enum {
	CTX_STOP       = 0x1,
	CTX_START      = 0x2,
	OPT_BACKGROUND = 0x4, // -b
	OPT_QUIET      = 0x8, // -q
	OPT_MAKEPID    = 0x10, // -m
	OPT_a          = 0x20, // -a
	OPT_n          = 0x40, // -n
	OPT_s          = 0x80, // -s
	OPT_u          = 0x100, // -u
	OPT_c          = 0x200, // -c
	OPT_x          = 0x400, // -x
	OPT_p          = 0x800, // -p
	OPT_OKNODO     = 0x1000 * ENABLE_FEATURE_START_STOP_DAEMON_FANCY, // -o
	OPT_VERBOSE    = 0x2000 * ENABLE_FEATURE_START_STOP_DAEMON_FANCY, // -v
	OPT_NICELEVEL  = 0x4000 * ENABLE_FEATURE_START_STOP_DAEMON_FANCY, // -N
};

int start_stop_daemon_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int start_stop_daemon_main(int argc, char **argv)
{
	unsigned opt;
	char *signame;
	char *startas;
	char *chuid;
#if ENABLE_FEATURE_START_STOP_DAEMON_FANCY
//	char *retry_arg = NULL;
//	int retries = -1;
	char *opt_N;
#endif
#if ENABLE_FEATURE_START_STOP_DAEMON_LONG_OPTIONS
	applet_long_options = start_stop_daemon_longopts;
#endif

	/* Check required one context option was given */
	opt_complementary = "K:S:K--S:S--K:m?p:K?xpun:S?xa";
	opt = getopt32(argv, "KSbqma:n:s:u:c:x:p:"
		USE_FEATURE_START_STOP_DAEMON_FANCY("ovN:"),
//		USE_FEATURE_START_STOP_DAEMON_FANCY("ovN:R:"),
		&startas, &cmdname, &signame, &userspec, &chuid, &execname, &pidfile
		USE_FEATURE_START_STOP_DAEMON_FANCY(,&opt_N)
//		USE_FEATURE_START_STOP_DAEMON_FANCY(,&retry_arg)
	);

	quiet = (opt & OPT_QUIET) && !(opt & OPT_VERBOSE);

	if (opt & OPT_s) {
		signal_nr = get_signum(signame);
		if (signal_nr < 0) bb_show_usage();
	}

	if (!(opt & OPT_a))
		startas = execname;

//	USE_FEATURE_START_STOP_DAEMON_FANCY(
//		if (retry_arg)
//			retries = xatoi_u(retry_arg);
//	)
	argc -= optind;
	argv += optind;

	if (userspec) {
		user_id = bb_strtou(userspec, NULL, 10);
		if (errno)
			user_id = xuname2uid(userspec);
	}

	if (opt & CTX_STOP) {
		int i = do_stop();
		return (opt & OPT_OKNODO) ? 0 : (i <= 0);
	}

	do_procinit();

	if (found) {
		if (!quiet)
			printf("%s already running\n%d\n", execname, found->pid);
		return !(opt & OPT_OKNODO);
	}
	*--argv = startas;
	if (opt & OPT_BACKGROUND) {
#if BB_MMU
		bb_daemonize(0);
#else
		pid_t pid = vfork();
		if (pid < 0) /* error */
			bb_perror_msg_and_die("vfork");
		if (pid != 0) {
			/* parent */
			/* why _exit? the child may have changed the stack,
			 * so "return 0" may do bad things */
			_exit(0);
		}
		/* child */
		setsid(); /* detach from controlling tty */
		/* Redirect stdio to /dev/null, close extra FDs.
		 * We do not actually daemonize because of DAEMON_ONLY_SANITIZE */
		bb_daemonize_or_rexec(
			DAEMON_DEVNULL_STDIO
			+ DAEMON_CLOSE_EXTRA_FDS
			+ DAEMON_ONLY_SANITIZE,
			NULL /* argv, unused */ );
#endif
	}
	if (opt & OPT_MAKEPID) {
		/* user wants _us_ to make the pidfile */
		write_pidfile(pidfile);
	}
	if (opt & OPT_c) {
		struct bb_uidgid_t ugid;
		parse_chown_usergroup_or_die(&ugid, chuid);
		if (ugid.gid != (gid_t) -1) xsetgid(ugid.gid);
		if (ugid.uid != (uid_t) -1) xsetuid(ugid.uid);
	}
#if ENABLE_FEATURE_START_STOP_DAEMON_FANCY
	if (opt & OPT_NICELEVEL) {
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
