/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */
#include "libbb.h"
#include <sys/mount.h>
#ifndef MS_SILENT
# define MS_SILENT      (1 << 15)
#endif
#ifndef MNT_DETACH
# define MNT_DETACH 0x00000002
#endif

#define BC_VERSION_STR "0.8"

/* For debugging, set to 0:
 * strace won't work with DO_SIGNAL_SYNC set to 1.
 */
#define DO_SIGNAL_SYNC 1


//Not supported: $PWD/bootchartd.conf and /etc/bootchartd.conf

//# tmpfs size
//# (32 MB should suffice for ~20 minutes worth of log data, but YMMV)
//TMPFS_SIZE=32m
//
//# Sampling period (in seconds)
//SAMPLE_PERIOD=0.2
//
//# Whether to enable and store BSD process accounting information.  The
//# kernel needs to be configured to enable v3 accounting
//# (CONFIG_BSD_PROCESS_ACCT_V3). accton from the GNU accounting utilities
//# is also required.
//PROCESS_ACCOUNTING="no"
//
//# Tarball for the various boot log files
//BOOTLOG_DEST=/var/log/bootchart.tgz
//
//# Whether to automatically stop logging as the boot process completes.
//# The logger will look for known processes that indicate bootup completion
//# at a specific runlevel (e.g. gdm-binary, mingetty, etc.).
//AUTO_STOP_LOGGER="yes"
//
//# Whether to automatically generate the boot chart once the boot logger
//# completes.  The boot chart will be generated in $AUTO_RENDER_DIR.
//# Note that the bootchart package must be installed.
//AUTO_RENDER="no"
//
//# Image format to use for the auto-generated boot chart
//# (choose between png, svg and eps).
//AUTO_RENDER_FORMAT="png"
//
//# Output directory for auto-generated boot charts
//AUTO_RENDER_DIR="/var/log"


/* Globals */
struct globals {
	char jiffy_line[sizeof(bb_common_bufsiz1)];
} FIX_ALIASING;
#define G (*(struct globals*)&bb_common_bufsiz1)
#define INIT_G() do { } while (0)

static void dump_file(FILE *fp, const char *filename)
{
	int fd = open(filename, O_RDONLY);
	if (fd >= 0) {
		fputs(G.jiffy_line, fp);
		fflush(fp);
		bb_copyfd_eof(fd, fileno(fp));
		close(fd);
		fputc('\n', fp);
	}
}

static int dump_procs(FILE *fp, int look_for_login_process)
{
	struct dirent *entry;
	DIR *dir = opendir("/proc");
	int found_login_process = 0;

	fputs(G.jiffy_line, fp);
	while ((entry = readdir(dir)) != NULL) {
		char name[sizeof("/proc/%u/cmdline") + sizeof(int)*3];
		int stat_fd;
		unsigned pid = bb_strtou(entry->d_name, NULL, 10);
		if (errno)
			continue;

		/* Android's version reads /proc/PID/cmdline and extracts
		 * non-truncated process name. Do we want to do that? */

		sprintf(name, "/proc/%u/stat", pid);
		stat_fd = open(name, O_RDONLY);
		if (stat_fd >= 0) {
			char *p;
			char stat_line[4*1024];
			int rd = safe_read(stat_fd, stat_line, sizeof(stat_line)-2);

			close(stat_fd);
			if (rd < 0)
				continue;
			stat_line[rd] = '\0';
			p = strchrnul(stat_line, '\n');
			*p++ = '\n';
			*p = '\0';
			fputs(stat_line, fp);
			if (!look_for_login_process)
				continue;
			p = strchr(stat_line, '(');
			if (!p)
				continue;
			p++;
			strchrnul(p, ')')[0] = '\0';
			/* If is gdm, kdm or a getty? */
			if (((p[0] == 'g' || p[0] == 'k' || p[0] == 'x') && p[1] == 'd' && p[2] == 'm')
			 || strstr(p, "getty")
			) {
				found_login_process = 1;
			}
		}
	}
	closedir(dir);
	fputc('\n', fp);
	return found_login_process;
}

static char *make_tempdir(const char *prog)
{
	char template[] = "/tmp/bootchart.XXXXXX";
	char *tempdir = xstrdup(mkdtemp(template));
	if (!tempdir) {
		/* /tmp is not writable (happens when we are used as init).
		 * Try to mount a tmpfs, them cd and lazily unmount it.
		 * Since we unmount it at once, we can mount it anywhere.
		 * Try a few locations which are likely ti exist.
		 */
		static const char dirs[] = "/mnt\0""/tmp\0""/boot\0""/proc\0";
		const char *try_dir = dirs;
		while (mount("none", try_dir, "tmpfs", MS_SILENT, "size=16m") != 0) {
			try_dir += strlen(try_dir) + 1;
			if (!try_dir[0])
				bb_perror_msg_and_die("can't %smount tmpfs", "");
		}
		//bb_error_msg("mounted tmpfs on %s", try_dir);
		xchdir(try_dir);
		if (umount2(try_dir, MNT_DETACH) != 0) {
			bb_perror_msg_and_die("can't %smount tmpfs", "un");
		}
	} else {
		xchdir(tempdir);
	}
	{
		FILE *header_fp = xfopen("header", "w");
		if (prog)
			fprintf(header_fp, "profile.process = %s\n", prog);
		fputs("version = "BC_VERSION_STR"\n", header_fp);
		fclose(header_fp);
	}

	return tempdir;
}

static void do_logging(void)
{
	//# Enable process accounting if configured
	//if [ "$PROCESS_ACCOUNTING" = "yes" ]; then
	//	[ -e kernel_pacct ] || : > kernel_pacct
	//	accton kernel_pacct
	//fi

	FILE *proc_stat = xfopen("proc_stat.log", "w");
	FILE *proc_diskstats = xfopen("proc_diskstats.log", "w");
	//FILE *proc_netdev = xfopen("proc_netdev.log", "w");
	FILE *proc_ps = xfopen("proc_ps.log", "w");
	int look_for_login_process = (getppid() == 1);
	unsigned count = 60*1000*1000 / (200*1000); /* ~1 minute */

	while (--count && !bb_got_signal) {
		char *p;
		int len = open_read_close("/proc/uptime", G.jiffy_line, sizeof(G.jiffy_line)-2);
		if (len < 0)
			goto wait_more;
		/* /proc/uptime has format "NNNNNN.MM NNNNNNN.MM" */
		/* we convert it to "NNNNNNMM\n" (using first value) */
		G.jiffy_line[len] = '\0';
		p = strchr(G.jiffy_line, '.');
		if (!p)
			goto wait_more;
		while (isdigit(*++p))
			p[-1] = *p;
		p[-1] = '\n';
		p[0] = '\0';

		dump_file(proc_stat, "/proc/stat");
		dump_file(proc_diskstats, "/proc/diskstats");
		//dump_file(proc_netdev, "/proc/net/dev");
		if (dump_procs(proc_ps, look_for_login_process)) {
			/* dump_procs saw a getty or {g,k,x}dm
			 * stop logging in 2 seconds:
			 */
			if (count > 2*1000*1000 / (200*1000))
				count = 2*1000*1000 / (200*1000);
		}
		fflush_all();
 wait_more:
		usleep(200*1000);
	}

	// [ -e kernel_pacct ] && accton off
}

static void finalize(char *tempdir)
{
	//# Stop process accounting if configured
	//local pacct=
	//[ -e kernel_pacct ] && pacct=kernel_pacct

	//(
	//	echo "version = $VERSION"
	//	echo "title = Boot chart for $( hostname | sed q ) ($( date ))"
	//	echo "system.uname = $( uname -srvm | sed q )"
	//	echo "system.release = $( sed q /etc/SuSE-release )"
	//	echo "system.cpu = $( grep '^model name' /proc/cpuinfo | sed q ) ($cpucount)"
	//	echo "system.kernel.options = $( sed q /proc/cmdline )"
	//) >> header

	/* Package log files */
	system("tar -zcf /var/log/bootchart.tgz header *.log"); // + $pacct
	/* Clean up (if we are not in detached tmpfs) */
	if (tempdir) {
		unlink("header");
		unlink("proc_stat.log");
		unlink("proc_diskstats.log");
		//unlink("proc_netdev.log");
		unlink("proc_ps.log");
		rmdir(tempdir);
	}

	/* shell-based bootchartd tries to run /usr/bin/bootchart if $AUTO_RENDER=yes:
	 * /usr/bin/bootchart -o "$AUTO_RENDER_DIR" -f $AUTO_RENDER_FORMAT "$BOOTLOG_DEST"
	 */
}

/* Usage:
 * bootchartd start [PROG ARGS]: start logging in background, USR1 stops it.
 *	With PROG, runs PROG, then kills background logging.
 * bootchartd stop: same as "killall -USR1 bootchartd"
 * bootchartd init: start logging in background
 *	Stop when getty/gdm is seen (if AUTO_STOP_LOGGER = yes).
 *	Meant to be used from init scripts.
 * bootchartd (pid==1): as init, but then execs $bootchart_init, /init, /sbin/init
 *	Meant to be used as kernel's init process.
 */
int bootchartd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int bootchartd_main(int argc UNUSED_PARAM, char **argv)
{
	pid_t parent_pid, logger_pid;
	smallint cmd;
	enum {
		CMD_STOP = 0,
		CMD_START,
		CMD_INIT,
		CMD_PID1, /* used to mark pid 1 case */
	};

	INIT_G();

	parent_pid = getpid();
	if (argv[1]) {
		cmd = index_in_strings("stop\0""start\0""init\0", argv[1]);
		if (cmd < 0)
			bb_show_usage();
		if (cmd == CMD_STOP) {
			pid_t *pidList = find_pid_by_name("bootchartd");
			while (*pidList != 0) {
				if (*pidList != parent_pid)
					kill(*pidList, SIGUSR1);
				pidList++;
			}
			return EXIT_SUCCESS;
		}
	} else {
		if (parent_pid != 1)
			bb_show_usage();
		cmd = CMD_PID1;
	}

	/* Here we are in START or INIT state. Create logger child: */
	logger_pid = fork_or_rexec(argv);

	if (logger_pid == 0) { /* child */
		char *tempdir;

		bb_signals(0
			+ (1 << SIGUSR1)
			+ (1 << SIGUSR2)
			+ (1 << SIGTERM)
			+ (1 << SIGQUIT)
			+ (1 << SIGINT)
			+ (1 << SIGHUP)
			, record_signo);

		if (DO_SIGNAL_SYNC)
			/* Inform parent that we are ready */
			raise(SIGSTOP);

		/* If we are started by kernel, PATH might be unset.
		 * In order to find "tar", let's set some sane PATH:
		 */
		if (cmd == CMD_PID1 && !getenv("PATH"))
			putenv((char*)bb_PATH_root_path);

		tempdir = make_tempdir(cmd == CMD_START ? argv[2] : NULL);
		do_logging();
		finalize(tempdir);
		return EXIT_SUCCESS;
	}

	/* parent */

	if (DO_SIGNAL_SYNC) {
		/* Wait for logger child to set handlers, then unpause it.
		 * Otherwise with short-lived PROG (e.g. "bootchartd start true")
		 * we might send SIGUSR1 before logger sets its handler.
		 */
		waitpid(logger_pid, NULL, WUNTRACED);
		kill(logger_pid, SIGCONT);
	}

	if (cmd == CMD_PID1) {
		char *bootchart_init = getenv("bootchart_init");
		if (bootchart_init)
			execl(bootchart_init, bootchart_init, NULL);
		execl("/init", "init", NULL);
		execl("/sbin/init", "init", NULL);
		bb_perror_msg_and_die("can't exec '%s'", "/sbin/init");
	}

	if (cmd == CMD_START && argv[2]) { /* "start PROG ARGS" */
		pid_t pid = vfork();
		if (pid < 0)
			bb_perror_msg_and_die("vfork");
		if (pid == 0) { /* child */
			argv += 2;
			execvp(argv[0], argv);
			bb_perror_msg_and_die("can't exec '%s'", argv[0]);
		}
		/* parent */
		waitpid(pid, NULL, 0);
		kill(logger_pid, SIGUSR1);
	}

	return EXIT_SUCCESS;
}
