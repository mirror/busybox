/* vi: set sw=4 ts=4: */
/*
 * Mini init implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Adjusted by so many folks, it's impossible to keep track.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include <paths.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <sys/reboot.h>

#if ENABLE_FEATURE_INIT_SYSLOG
# include <sys/syslog.h>
#endif

#define INIT_BUFFS_SIZE 256
#define CONSOLE_NAME_SIZE 32
#define MAXENV	16		/* Number of env. vars */

#ifndef _PATH_STDPATH
#define _PATH_STDPATH	"/usr/bin:/bin:/usr/sbin:/sbin"
#endif

#if ENABLE_FEATURE_INIT_COREDUMPS
/*
 * When a file named CORE_ENABLE_FLAG_FILE exists, setrlimit is called
 * before processes are spawned to set core file size as unlimited.
 * This is for debugging only.  Don't use this is production, unless
 * you want core dumps lying about....
 */
#define CORE_ENABLE_FLAG_FILE "/.init_enable_core"
#include <sys/resource.h>
#endif

#define INITTAB      "/etc/inittab"	/* inittab file location */
#ifndef INIT_SCRIPT
#define INIT_SCRIPT  "/etc/init.d/rcS"	/* Default sysinit script. */
#endif

/* Allowed init action types */
#define SYSINIT     0x001
#define RESPAWN     0x002
#define ASKFIRST    0x004
#define WAIT        0x008
#define ONCE        0x010
#define CTRLALTDEL  0x020
#define SHUTDOWN    0x040
#define RESTART     0x080

/* A mapping between "inittab" action name strings and action type codes. */
struct init_action_type {
	const char *name;
	int action;
};

static const struct init_action_type actions[] = {
	{"sysinit", SYSINIT},
	{"respawn", RESPAWN},
	{"askfirst", ASKFIRST},
	{"wait", WAIT},
	{"once", ONCE},
	{"ctrlaltdel", CTRLALTDEL},
	{"shutdown", SHUTDOWN},
	{"restart", RESTART},
	{0, 0}
};

/* Set up a linked list of init_actions, to be read from inittab */
struct init_action {
	struct init_action *next;
	int action;
	pid_t pid;
	char command[INIT_BUFFS_SIZE];
	char terminal[CONSOLE_NAME_SIZE];
};

/* Static variables */
static struct init_action *init_action_list = NULL;

#if !ENABLE_FEATURE_INIT_SYSLOG
static const char *log_console = VC_5;
#endif
#if !ENABLE_DEBUG_INIT
static sig_atomic_t got_cont = 0;
#endif

enum {
	L_LOG = 0x1,
	L_CONSOLE = 0x2,

#if ENABLE_FEATURE_EXTRA_QUIET
	MAYBE_CONSOLE = 0x0,
#else
	MAYBE_CONSOLE = L_CONSOLE,
#endif

#ifndef RB_HALT_SYSTEM
	RB_HALT_SYSTEM = 0xcdef0123, /* FIXME: this overflows enum */
	RB_ENABLE_CAD = 0x89abcdef,
	RB_DISABLE_CAD = 0,
	RB_POWER_OFF = 0x4321fedc,
	RB_AUTOBOOT = 0x01234567,
#endif
};

static const char * const environment[] = {
	"HOME=/",
	"PATH=" _PATH_STDPATH,
	"SHELL=/bin/sh",
	"USER=root",
	NULL
};

/* Function prototypes */
static void delete_init_action(struct init_action *a);
static int waitfor(const struct init_action *a, pid_t pid);
#if !ENABLE_DEBUG_INIT
static void shutdown_signal(int sig);
#endif

#if !ENABLE_DEBUG_INIT
static void loop_forever(void)
{
	while (1)
		sleep(1);
}
#endif

/* Print a message to the specified device.
 * Device may be bitwise-or'd from L_LOG | L_CONSOLE */
#if ENABLE_DEBUG_INIT
#define messageD message
#else
#define messageD(...)  do {} while (0)
#endif
static void message(int device, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));
static void message(int device, const char *fmt, ...)
{
#if !ENABLE_FEATURE_INIT_SYSLOG
	static int log_fd = -1;
#endif

	va_list arguments;
	int l;
	char msg[128];

	msg[0] = '\r';
	va_start(arguments, fmt);
	vsnprintf(msg + 1, sizeof(msg) - 2, fmt, arguments);
	va_end(arguments);
	msg[sizeof(msg) - 2] = '\0';
	l = strlen(msg);

#if ENABLE_FEATURE_INIT_SYSLOG
	/* Log the message to syslogd */
	if (device & L_LOG) {
		/* don't out "\r" */
		openlog(applet_name, 0, LOG_DAEMON);
		syslog(LOG_INFO, "init: %s", msg + 1);
		closelog();
	}
	msg[l++] = '\n';
	msg[l] = '\0';
#else
	msg[l++] = '\n';
	msg[l] = '\0';
	/* Take full control of the log tty, and never close it.
	 * It's mine, all mine!  Muhahahaha! */
	if (log_fd < 0) {
		if (!log_console) {
			log_fd = 2;
		} else {
			log_fd = device_open(log_console, O_WRONLY | O_NONBLOCK | O_NOCTTY);
			if (log_fd < 0) {
				bb_error_msg("can't log to %s", log_console);
				device = L_CONSOLE;
			} else {
				fcntl(log_fd, F_SETFD, FD_CLOEXEC);
			}
		}
	}
	if (device & L_LOG) {
		full_write(log_fd, msg, l);
		if (log_fd == 2)
			return; /* don't print dup messages */
	}
#endif

	if (device & L_CONSOLE) {
		/* Send console messages to console so people will see them. */
		full_write(2, msg, l);
	}
}

/* Set terminal settings to reasonable defaults */
static void set_sane_term(void)
{
	struct termios tty;

	tcgetattr(STDIN_FILENO, &tty);

	/* set control chars */
	tty.c_cc[VINTR] = 3;	/* C-c */
	tty.c_cc[VQUIT] = 28;	/* C-\ */
	tty.c_cc[VERASE] = 127;	/* C-? */
	tty.c_cc[VKILL] = 21;	/* C-u */
	tty.c_cc[VEOF] = 4;	/* C-d */
	tty.c_cc[VSTART] = 17;	/* C-q */
	tty.c_cc[VSTOP] = 19;	/* C-s */
	tty.c_cc[VSUSP] = 26;	/* C-z */

	/* use line dicipline 0 */
	tty.c_line = 0;

	/* Make it be sane */
	tty.c_cflag &= CBAUD | CBAUDEX | CSIZE | CSTOPB | PARENB | PARODD;
	tty.c_cflag |= CREAD | HUPCL | CLOCAL;

	/* input modes */
	tty.c_iflag = ICRNL | IXON | IXOFF;

	/* output modes */
	tty.c_oflag = OPOST | ONLCR;

	/* local modes */
	tty.c_lflag =
		ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN;

	tcsetattr(STDIN_FILENO, TCSANOW, &tty);
}

/* From <linux/serial.h> */
struct serial_struct {
	int	type;
	int	line;
	unsigned int	port;
	int	irq;
	int	flags;
	int	xmit_fifo_size;
	int	custom_divisor;
	int	baud_base;
	unsigned short	close_delay;
	char	io_type;
	char	reserved_char[1];
	int	hub6;
	unsigned short	closing_wait; /* time to wait before closing */
	unsigned short	closing_wait2; /* no longer used... */
	unsigned char	*iomem_base;
	unsigned short	iomem_reg_shift;
	unsigned int	port_high;
	unsigned long	iomap_base;	/* cookie passed into ioremap */
	int	reserved[1];
	/* Paranoia (imagine 64bit kernel overwriting 32bit userspace stack) */
	uint32_t bbox_reserved[16];
};
static void console_init(void)
{
	struct serial_struct sr;
	char *s;

	s = getenv("CONSOLE");
	if (!s) s = getenv("console");
	if (s) {
		int fd = open(s, O_RDWR | O_NONBLOCK | O_NOCTTY);
		if (fd >= 0) {
			dup2(fd, 0);
			dup2(fd, 1);
			dup2(fd, 2);
			while (fd > 2) close(fd--);
		}
		messageD(L_LOG, "console='%s'", s);
	}

	s = getenv("TERM");
	if (ioctl(0, TIOCGSERIAL, &sr) == 0) {
		/* Force the TERM setting to vt102 for serial console --
		 * if TERM is set to linux (the default) */
		if (!s || strcmp(s, "linux") == 0)
			putenv((char*)"TERM=vt102");
#if !ENABLE_FEATURE_INIT_SYSLOG
		log_console = NULL;
#endif
	} else if (!s)
		putenv((char*)"TERM=linux");
}

static void fixup_argv(int argc, char **argv, const char *new_argv0)
{
	int len;

	/* Fix up argv[0] to be certain we claim to be init */
	len = strlen(argv[0]);
	strncpy(argv[0], new_argv0, len);

	/* Wipe argv[1]-argv[N] so they don't clutter the ps listing */
	len = 1;
	while (argc > len) {
		memset(argv[len], 0, strlen(argv[len]));
		len++;
	}
}

/* Open the new terminal device */
static void open_stdio_to_tty(const char* tty_name, int fail)
{
	/* empty tty_name means "use init's tty", else... */
	if (tty_name[0]) {
		int fd = device_open(tty_name, O_RDWR);
		if (fd < 0) {
			message(L_LOG | L_CONSOLE, "Can't open %s: %s",
				tty_name, strerror(errno));
			if (fail)
				_exit(1);
#if !ENABLE_DEBUG_INIT
			shutdown_signal(SIGUSR1);
#else
			_exit(2);
#endif
		} else {
			dup2(fd, 0);
			dup2(fd, 1);
			dup2(fd, 2);
			if (fd > 2) close(fd);
		}
	}
	set_sane_term();
}

static pid_t run(const struct init_action *a)
{
	int i;
	pid_t pid;
	char *s, *tmpCmd, *cmdpath;
	char *cmd[INIT_BUFFS_SIZE];
	char buf[INIT_BUFFS_SIZE + 6];	/* INIT_BUFFS_SIZE+strlen("exec ")+1 */
	sigset_t nmask, omask;

	/* Block sigchild while forking.  */
	sigemptyset(&nmask);
	sigaddset(&nmask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &nmask, &omask);
	pid = fork();
	sigprocmask(SIG_SETMASK, &omask, NULL);

	if (pid)
		return pid;

	/* Reset signal handlers that were set by the parent process */
	signal(SIGUSR1, SIG_DFL);
	signal(SIGUSR2, SIG_DFL);
	signal(SIGINT, SIG_DFL);
	signal(SIGTERM, SIG_DFL);
	signal(SIGHUP, SIG_DFL);
	signal(SIGQUIT, SIG_DFL);
	signal(SIGCONT, SIG_DFL);
	signal(SIGSTOP, SIG_DFL);
	signal(SIGTSTP, SIG_DFL);

	/* Create a new session and make ourself the process
	 * group leader */
	setsid();

	/* Open the new terminal device */
	open_stdio_to_tty(a->terminal, 1);

	/* If the init Action requires us to wait, then force the
	 * supplied terminal to be the controlling tty. */
	if (a->action & (SYSINIT | WAIT | CTRLALTDEL | SHUTDOWN | RESTART)) {

		/* Now fork off another process to just hang around */
		if ((pid = fork()) < 0) {
			message(L_LOG | L_CONSOLE, "Can't fork");
			_exit(1);
		}

		if (pid > 0) {

			/* We are the parent -- wait till the child is done */
			signal(SIGINT, SIG_IGN);
			signal(SIGTSTP, SIG_IGN);
			signal(SIGQUIT, SIG_IGN);
			signal(SIGCHLD, SIG_DFL);

			waitfor(NULL, pid);
			/* See if stealing the controlling tty back is necessary */
			if (tcgetpgrp(0) != getpid())
				_exit(0);

			/* Use a temporary process to steal the controlling tty. */
			if ((pid = fork()) < 0) {
				message(L_LOG | L_CONSOLE, "Can't fork");
				_exit(1);
			}
			if (pid == 0) {
				setsid();
				ioctl(0, TIOCSCTTY, 1);
				_exit(0);
			}
			waitfor(NULL, pid);
			_exit(0);
		}

		/* Now fall though to actually execute things */
	}

	/* See if any special /bin/sh requiring characters are present */
	if (strpbrk(a->command, "~`!$^&*()=|\\{}[];\"'<>?") != NULL) {
		cmd[0] = (char*)DEFAULT_SHELL;
		cmd[1] = (char*)"-c";
		cmd[2] = strcat(strcpy(buf, "exec "), a->command);
		cmd[3] = NULL;
	} else {
		/* Convert command (char*) into cmd (char**, one word per string) */
		strcpy(buf, a->command);
		s = buf;
		for (tmpCmd = buf, i = 0; (tmpCmd = strsep(&s, " \t")) != NULL;) {
			if (*tmpCmd != '\0') {
				cmd[i] = tmpCmd;
				i++;
			}
		}
		cmd[i] = NULL;
	}

	cmdpath = cmd[0];

	/*
	 * Interactive shells want to see a dash in argv[0].  This
	 * typically is handled by login, argv will be setup this
	 * way if a dash appears at the front of the command path
	 * (like "-/bin/sh").
	 */
	if (*cmdpath == '-') {
		/* skip over the dash */
		++cmdpath;

		/* find the last component in the command pathname */
		s = bb_get_last_path_component(cmdpath);

		/* make a new argv[0] */
		if ((cmd[0] = malloc(strlen(s) + 2)) == NULL) {
			message(L_LOG | L_CONSOLE, bb_msg_memory_exhausted);
			cmd[0] = cmdpath;
		} else {
			cmd[0][0] = '-';
			strcpy(cmd[0] + 1, s);
		}
#if ENABLE_FEATURE_INIT_SCTTY
		/* Establish this process as session leader and
		 * (attempt) to make the tty (if any) a controlling tty.
		 */
		setsid();
		ioctl(0, TIOCSCTTY, 0 /*don't steal it*/);
#endif
	}

#if !defined(__UCLIBC__) || defined(__ARCH_HAS_MMU__)
	if (a->action & ASKFIRST) {
		static const char press_enter[] =
#ifdef CUSTOMIZED_BANNER
#include CUSTOMIZED_BANNER
#endif
			"\nPlease press Enter to activate this console. ";
		char c;
		/*
		 * Save memory by not exec-ing anything large (like a shell)
		 * before the user wants it. This is critical if swap is not
		 * enabled and the system has low memory. Generally this will
		 * be run on the second virtual console, and the first will
		 * be allowed to start a shell or whatever an init script
		 * specifies.
		 */
		messageD(L_LOG, "waiting for enter to start '%s'"
					"(pid %d, tty '%s')\n",
				  cmdpath, getpid(), a->terminal);
		full_write(1, press_enter, sizeof(press_enter) - 1);
		while (read(0, &c, 1) == 1 && c != '\n')
			;
	}
#endif
	/* Log the process name and args */
	message(L_LOG, "starting pid %d, tty '%s': '%s'",
			  getpid(), a->terminal, cmdpath);

#if ENABLE_FEATURE_INIT_COREDUMPS
	{
		struct stat sb;
		if (stat(CORE_ENABLE_FLAG_FILE, &sb) == 0) {
			struct rlimit limit;

			limit.rlim_cur = RLIM_INFINITY;
			limit.rlim_max = RLIM_INFINITY;
			setrlimit(RLIMIT_CORE, &limit);
		}
	}
#endif
	/* Now run it.  The new program will take over this PID,
	 * so nothing further in init.c should be run. */
	BB_EXECVP(cmdpath, cmd);

	/* We're still here?  Some error happened. */
	message(L_LOG | L_CONSOLE, "Cannot run '%s': %s",
			cmdpath, strerror(errno));
	_exit(-1);
}

static int waitfor(const struct init_action *a, pid_t pid)
{
	int runpid;
	int status, wpid;

	runpid = (NULL == a)? pid : run(a);
	while (1) {
		wpid = waitpid(runpid, &status, 0);
		if (wpid == runpid)
			break;
		if (wpid == -1 && errno == ECHILD) {
			/* we missed its termination */
			break;
		}
		/* FIXME other errors should maybe trigger an error, but allow
		 * the program to continue */
	}
	return wpid;
}

/* Run all commands of a particular type */
static void run_actions(int action)
{
	struct init_action *a, *tmp;

	for (a = init_action_list; a; a = tmp) {
		tmp = a->next;
		if (a->action == action) {
			/* a->terminal of "" means "init's console" */
			if (a->terminal[0] && access(a->terminal, R_OK | W_OK)) {
				delete_init_action(a);
			} else if (a->action & (SYSINIT | WAIT | CTRLALTDEL | SHUTDOWN | RESTART)) {
				waitfor(a, 0);
				delete_init_action(a);
			} else if (a->action & ONCE) {
				run(a);
				delete_init_action(a);
			} else if (a->action & (RESPAWN | ASKFIRST)) {
				/* Only run stuff with pid==0.  If they have
				 * a pid, that means it is still running */
				if (a->pid == 0) {
					a->pid = run(a);
				}
			}
		}
	}
}

#if !ENABLE_DEBUG_INIT
static void init_reboot(unsigned long magic)
{
	pid_t pid;
	/* We have to fork here, since the kernel calls do_exit(0) in
	 * linux/kernel/sys.c, which can cause the machine to panic when
	 * the init process is killed.... */
	pid = vfork();
	if (pid == 0) { /* child */
		reboot(magic);
		_exit(0);
	}
	waitpid(pid, NULL, 0);
}

static void shutdown_system(void)
{
	sigset_t block_signals;

	/* run everything to be run at "shutdown".  This is done _prior_
	 * to killing everything, in case people wish to use scripts to
	 * shut things down gracefully... */
	run_actions(SHUTDOWN);

	/* first disable all our signals */
	sigemptyset(&block_signals);
	sigaddset(&block_signals, SIGHUP);
	sigaddset(&block_signals, SIGQUIT);
	sigaddset(&block_signals, SIGCHLD);
	sigaddset(&block_signals, SIGUSR1);
	sigaddset(&block_signals, SIGUSR2);
	sigaddset(&block_signals, SIGINT);
	sigaddset(&block_signals, SIGTERM);
	sigaddset(&block_signals, SIGCONT);
	sigaddset(&block_signals, SIGSTOP);
	sigaddset(&block_signals, SIGTSTP);
	sigprocmask(SIG_BLOCK, &block_signals, NULL);

	message(L_CONSOLE | L_LOG, "The system is going down NOW!");

	/* Allow Ctrl-Alt-Del to reboot system. */
	init_reboot(RB_ENABLE_CAD);

	/* Send signals to every process _except_ pid 1 */
	message(L_CONSOLE | L_LOG, "Sending SIG%s to all processes", "TERM");
	kill(-1, SIGTERM);
	sync();
	sleep(1);

	message(L_CONSOLE | L_LOG, "Sending SIG%s to all processes", "KILL");
	kill(-1, SIGKILL);
	sync();
	sleep(1);
}

static void exec_signal(int sig ATTRIBUTE_UNUSED)
{
	struct init_action *a, *tmp;
	sigset_t unblock_signals;

	for (a = init_action_list; a; a = tmp) {
		tmp = a->next;
		if (a->action & RESTART) {
			shutdown_system();

			/* unblock all signals, blocked in shutdown_system() */
			sigemptyset(&unblock_signals);
			sigaddset(&unblock_signals, SIGHUP);
			sigaddset(&unblock_signals, SIGQUIT);
			sigaddset(&unblock_signals, SIGCHLD);
			sigaddset(&unblock_signals, SIGUSR1);
			sigaddset(&unblock_signals, SIGUSR2);
			sigaddset(&unblock_signals, SIGINT);
			sigaddset(&unblock_signals, SIGTERM);
			sigaddset(&unblock_signals, SIGCONT);
			sigaddset(&unblock_signals, SIGSTOP);
			sigaddset(&unblock_signals, SIGTSTP);
			sigprocmask(SIG_UNBLOCK, &unblock_signals, NULL);

			/* Open the new terminal device */
			open_stdio_to_tty(a->terminal, 0);

			messageD(L_CONSOLE | L_LOG, "Trying to re-exec %s", a->command);
			BB_EXECLP(a->command, a->command, NULL);

			message(L_CONSOLE | L_LOG, "Cannot run '%s': %s",
					a->command, strerror(errno));
			sleep(2);
			init_reboot(RB_HALT_SYSTEM);
			loop_forever();
		}
	}
}

static void shutdown_signal(int sig)
{
	const char *m;
	int rb;

	shutdown_system();

	m = "halt";
	rb = RB_HALT_SYSTEM;
	if (sig == SIGTERM) {
		m = "reboot";
		rb = RB_AUTOBOOT;
	} else if (sig == SIGUSR2) {
		m = "poweroff";
		rb = RB_POWER_OFF;
	}
	message(L_CONSOLE | L_LOG, "Requesting system %s", m);
	/* allow time for last message to reach serial console */
	sleep(2);
	init_reboot(rb);
	loop_forever();
}

static void ctrlaltdel_signal(int sig ATTRIBUTE_UNUSED)
{
	run_actions(CTRLALTDEL);
}

/* The SIGSTOP & SIGTSTP handler */
static void stop_handler(int sig ATTRIBUTE_UNUSED)
{
	int saved_errno = errno;

	got_cont = 0;
	while (!got_cont)
		pause();
	got_cont = 0;
	errno = saved_errno;
}

/* The SIGCONT handler */
static void cont_handler(int sig ATTRIBUTE_UNUSED)
{
	got_cont = 1;
}

#endif	/* !ENABLE_DEBUG_INIT */

static void new_init_action(int action, const char *command, const char *cons)
{
	struct init_action *new_action, *a, *last;

	if (strcmp(cons, bb_dev_null) == 0 && (action & ASKFIRST))
		return;

	/* Append to the end of the list */
	for (a = last = init_action_list; a; a = a->next) {
		/* don't enter action if it's already in the list,
		 * but do overwrite existing actions */
		if ((strcmp(a->command, command) == 0)
		 && (strcmp(a->terminal, cons) == 0)
		) {
			a->action = action;
			return;
		}
		last = a;
	}

	new_action = xzalloc(sizeof(struct init_action));
	if (last) {
		last->next = new_action;
	} else {
		init_action_list = new_action;
	}
	strcpy(new_action->command, command);
	new_action->action = action;
	strcpy(new_action->terminal, cons);
	messageD(L_LOG | L_CONSOLE, "command='%s' action=%d tty='%s'\n",
		new_action->command, new_action->action, new_action->terminal);
}

static void delete_init_action(struct init_action *action)
{
	struct init_action *a, *b = NULL;

	for (a = init_action_list; a; b = a, a = a->next) {
		if (a == action) {
			if (b == NULL) {
				init_action_list = a->next;
			} else {
				b->next = a->next;
			}
			free(a);
			break;
		}
	}
}

/* NOTE that if CONFIG_FEATURE_USE_INITTAB is NOT defined,
 * then parse_inittab() simply adds in some default
 * actions(i.e., runs INIT_SCRIPT and then starts a pair
 * of "askfirst" shells).  If CONFIG_FEATURE_USE_INITTAB
 * _is_ defined, but /etc/inittab is missing, this
 * results in the same set of default behaviors.
 */
static void parse_inittab(void)
{
#if ENABLE_FEATURE_USE_INITTAB
	FILE *file;
	char buf[INIT_BUFFS_SIZE], lineAsRead[INIT_BUFFS_SIZE];
	char tmpConsole[CONSOLE_NAME_SIZE];
	char *id, *runlev, *action, *command, *eol;
	const struct init_action_type *a = actions;

	file = fopen(INITTAB, "r");
	if (file == NULL) {
		/* No inittab file -- set up some default behavior */
#endif
		/* Reboot on Ctrl-Alt-Del */
		new_init_action(CTRLALTDEL, "reboot", "");
		/* Umount all filesystems on halt/reboot */
		new_init_action(SHUTDOWN, "umount -a -r", "");
		/* Swapoff on halt/reboot */
		if (ENABLE_SWAPONOFF) new_init_action(SHUTDOWN, "swapoff -a", "");
		/* Prepare to restart init when a HUP is received */
		new_init_action(RESTART, "init", "");
		/* Askfirst shell on tty1-4 */
		new_init_action(ASKFIRST, bb_default_login_shell, "");
		new_init_action(ASKFIRST, bb_default_login_shell, VC_2);
		new_init_action(ASKFIRST, bb_default_login_shell, VC_3);
		new_init_action(ASKFIRST, bb_default_login_shell, VC_4);
		/* sysinit */
		new_init_action(SYSINIT, INIT_SCRIPT, "");

		return;
#if ENABLE_FEATURE_USE_INITTAB
	}

	while (fgets(buf, INIT_BUFFS_SIZE, file) != NULL) {
		/* Skip leading spaces */
		for (id = buf; *id == ' ' || *id == '\t'; id++);

		/* Skip the line if it's a comment */
		if (*id == '#' || *id == '\n')
			continue;

		/* Trim the trailing \n */
		//XXX: chomp() ?
		eol = strrchr(id, '\n');
		if (eol != NULL)
			*eol = '\0';

		/* Keep a copy around for posterity's sake (and error msgs) */
		strcpy(lineAsRead, buf);

		/* Separate the ID field from the runlevels */
		runlev = strchr(id, ':');
		if (runlev == NULL || *(runlev + 1) == '\0') {
			message(L_LOG | L_CONSOLE, "Bad inittab entry: %s", lineAsRead);
			continue;
		} else {
			*runlev = '\0';
			++runlev;
		}

		/* Separate the runlevels from the action */
		action = strchr(runlev, ':');
		if (action == NULL || *(action + 1) == '\0') {
			message(L_LOG | L_CONSOLE, "Bad inittab entry: %s", lineAsRead);
			continue;
		} else {
			*action = '\0';
			++action;
		}

		/* Separate the action from the command */
		command = strchr(action, ':');
		if (command == NULL || *(command + 1) == '\0') {
			message(L_LOG | L_CONSOLE, "Bad inittab entry: %s", lineAsRead);
			continue;
		} else {
			*command = '\0';
			++command;
		}

		/* Ok, now process it */
		for (a = actions; a->name != 0; a++) {
			if (strcmp(a->name, action) == 0) {
				if (*id != '\0') {
					if(strncmp(id, "/dev/", 5) == 0)
						id += 5;
					strcpy(tmpConsole, "/dev/");
					safe_strncpy(tmpConsole + 5, id,
						sizeof(tmpConsole) - 5);
					id = tmpConsole;
				}
				new_init_action(a->action, command, id);
				break;
			}
		}
		if (a->name == 0) {
			/* Choke on an unknown action */
			message(L_LOG | L_CONSOLE, "Bad inittab entry: %s", lineAsRead);
		}
	}
	fclose(file);
#endif /* FEATURE_USE_INITTAB */
}

#if ENABLE_FEATURE_USE_INITTAB
static void reload_signal(int sig ATTRIBUTE_UNUSED)
{
	struct init_action *a, *tmp;

	message(L_LOG, "reloading /etc/inittab");

	/* disable old entrys */
	for (a = init_action_list; a; a = a->next ) {
		a->action = ONCE;
	}

	parse_inittab();

	/* remove unused entrys */
	for (a = init_action_list; a; a = tmp) {
		tmp = a->next;
		if (a->action & (ONCE | SYSINIT | WAIT ) &&
				a->pid == 0 ) {
			delete_init_action(a);
		}
	}
	run_actions(RESPAWN);
}
#endif  /* FEATURE_USE_INITTAB */

int init_main(int argc, char **argv);
int init_main(int argc, char **argv)
{
	struct init_action *a;
	pid_t wpid;

	die_sleep = 30 * 24*60*60; /* if xmalloc will ever die... */

	if (argc > 1 && !strcmp(argv[1], "-q")) {
		return kill(1, SIGHUP);
	}
#if !ENABLE_DEBUG_INIT
	/* Expect to be invoked as init with PID=1 or be invoked as linuxrc */
	if (getpid() != 1
	 && (!ENABLE_FEATURE_INITRD || !strstr(applet_name, "linuxrc"))
	) {
		bb_show_usage();
	}
	/* Set up sig handlers  -- be sure to
	 * clear all of these in run() */
	signal(SIGHUP, exec_signal);
	signal(SIGQUIT, exec_signal);
	signal(SIGUSR1, shutdown_signal);
	signal(SIGUSR2, shutdown_signal);
	signal(SIGINT, ctrlaltdel_signal);
	signal(SIGTERM, shutdown_signal);
	signal(SIGCONT, cont_handler);
	signal(SIGSTOP, stop_handler);
	signal(SIGTSTP, stop_handler);

	/* Turn off rebooting via CTL-ALT-DEL -- we get a
	 * SIGINT on CAD so we can shut things down gracefully... */
	init_reboot(RB_DISABLE_CAD);
#endif

	/* Figure out where the default console should be */
	console_init();
	set_sane_term();
	chdir("/");
	setsid();
	{
		const char * const *e;
		/* Make sure environs is set to something sane */
		for (e = environment; *e; e++)
			putenv((char *) *e);
	}

	if (argc > 1) setenv("RUNLEVEL", argv[1], 1);

	/* Hello world */
	message(MAYBE_CONSOLE | L_LOG, "init started: %s", bb_msg_full_version);

	/* Make sure there is enough memory to do something useful. */
	if (ENABLE_SWAPONOFF) {
		struct sysinfo info;

		if (!sysinfo(&info) &&
			(info.mem_unit ? : 1) * (long long)info.totalram < 1024*1024)
		{
			message(L_CONSOLE, "Low memory, forcing swapon");
			/* swapon -a requires /proc typically */
			new_init_action(SYSINIT, "mount -t proc proc /proc", "");
			/* Try to turn on swap */
			new_init_action(SYSINIT, "swapon -a", "");
			run_actions(SYSINIT);   /* wait and removing */
		}
	}

	/* Check if we are supposed to be in single user mode */
	if (argc > 1
	 && (!strcmp(argv[1], "single") || !strcmp(argv[1], "-s") || LONE_CHAR(argv[1], '1'))
	) {
		/* Start a shell on console */
		new_init_action(RESPAWN, bb_default_login_shell, "");
	} else {
		/* Not in single user mode -- see what inittab says */

		/* NOTE that if CONFIG_FEATURE_USE_INITTAB is NOT defined,
		 * then parse_inittab() simply adds in some default
		 * actions(i.e., runs INIT_SCRIPT and then starts a pair
		 * of "askfirst" shells */
		parse_inittab();
	}

#if ENABLE_SELINUX
	if (getenv("SELINUX_INIT") == NULL) {
		int enforce = 0;

		putenv("SELINUX_INIT=YES");
		if (selinux_init_load_policy(&enforce) == 0) {
			BB_EXECVP(argv[0], argv);
		} else if (enforce > 0) {
			/* SELinux in enforcing mode but load_policy failed */
			/* At this point, we probably can't open /dev/console, so log() won't work */
			message(L_CONSOLE, "Cannot load SELinux Policy. "
				"Machine is in enforcing mode. Halting now.");
			exit(1);
		}
	}
#endif /* CONFIG_SELINUX */

	/* Make the command line just say "init"  -- thats all, nothing else */
	fixup_argv(argc, argv, "init");

	/* Now run everything that needs to be run */

	/* First run the sysinit command */
	run_actions(SYSINIT);

	/* Next run anything that wants to block */
	run_actions(WAIT);

	/* Next run anything to be run only once */
	run_actions(ONCE);

#if ENABLE_FEATURE_USE_INITTAB
	/* Redefine SIGHUP to reread /etc/inittab */
	signal(SIGHUP, reload_signal);
#else
	signal(SIGHUP, SIG_IGN);
#endif /* FEATURE_USE_INITTAB */

	/* Now run the looping stuff for the rest of forever */
	while (1) {
		/* run the respawn stuff */
		run_actions(RESPAWN);

		/* run the askfirst stuff */
		run_actions(ASKFIRST);

		/* Don't consume all CPU time -- sleep a bit */
		sleep(1);

		/* Wait for a child process to exit */
		wpid = wait(NULL);
		while (wpid > 0) {
			/* Find out who died and clean up their corpse */
			for (a = init_action_list; a; a = a->next) {
				if (a->pid == wpid) {
					/* Set the pid to 0 so that the process gets
					 * restarted by run_actions() */
					a->pid = 0;
					message(L_LOG, "process '%s' (pid %d) exited. "
							"Scheduling it for restart.",
							a->command, wpid);
				}
			}
			/* see if anyone else is waiting to be reaped */
			wpid = waitpid(-1, NULL, WNOHANG);
		}
	}
}
