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

#include "libbb.h"
#include <syslog.h>
#include <paths.h>
#include <sys/reboot.h>

#define COMMAND_SIZE 256
#define CONSOLE_NAME_SIZE 32
#define MAXENV	16		/* Number of env. vars */

/*
 * When a file named CORE_ENABLE_FLAG_FILE exists, setrlimit is called
 * before processes are spawned to set core file size as unlimited.
 * This is for debugging only.  Don't use this is production, unless
 * you want core dumps lying about....
 */
#define CORE_ENABLE_FLAG_FILE "/.init_enable_core"
#include <sys/resource.h>

#define INITTAB      "/etc/inittab"	/* inittab file location */
#ifndef INIT_SCRIPT
#define INIT_SCRIPT  "/etc/init.d/rcS"	/* Default sysinit script. */
#endif

/* Allowed init action types */
#define SYSINIT     0x01
#define RESPAWN     0x02
/* like respawn, but wait for <Enter> to be pressed on tty: */
#define ASKFIRST    0x04
#define WAIT        0x08
#define ONCE        0x10
#define CTRLALTDEL  0x20
#define SHUTDOWN    0x40
#define RESTART     0x80

#define STR_SYSINIT     "\x01"
#define STR_RESPAWN     "\x02"
#define STR_ASKFIRST    "\x04"
#define STR_WAIT        "\x08"
#define STR_ONCE        "\x10"
#define STR_CTRLALTDEL  "\x20"
#define STR_SHUTDOWN    "\x40"
#define STR_RESTART     "\x80"

/* Set up a linked list of init_actions, to be read from inittab */
struct init_action {
	struct init_action *next;
	pid_t pid;
	uint8_t action_type;
	char terminal[CONSOLE_NAME_SIZE];
	char command[COMMAND_SIZE];
};

/* Static variables */
static struct init_action *init_action_list = NULL;

static const char *log_console = VC_5;
static sig_atomic_t got_cont = 0;

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

static const char *const environment[] = {
	"HOME=/",
	bb_PATH_root_path,
	"SHELL=/bin/sh",
	"USER=root",
	NULL
};

/* Function prototypes */
static void delete_init_action(struct init_action *a);
static void halt_reboot_pwoff(int sig) ATTRIBUTE_NORETURN;

static void waitfor(pid_t pid)
{
	/* waitfor(run(x)): protect against failed fork inside run() */
	if (pid <= 0)
		return;

	/* Wait for any child (prevent zombies from exiting orphaned processes)
	 * but exit the loop only when specified one has exited. */
	while (wait(NULL) != pid)
		continue;
}

static void loop_forever(void) ATTRIBUTE_NORETURN;
static void loop_forever(void)
{
	while (1)
		sleep(1);
}

/* Print a message to the specified device.
 * "where" may be bitwise-or'd from L_LOG | L_CONSOLE
 * NB: careful, we can be called after vfork!
 */
#define messageD(...) do { if (ENABLE_DEBUG_INIT) message(__VA_ARGS__); } while (0)
static void message(int where, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));
static void message(int where, const char *fmt, ...)
{
	static int log_fd = -1;
	va_list arguments;
	int l;
	char msg[128];

	msg[0] = '\r';
	va_start(arguments, fmt);
	vsnprintf(msg + 1, sizeof(msg) - 2, fmt, arguments);
	va_end(arguments);
	msg[sizeof(msg) - 2] = '\0';
	l = strlen(msg);

	if (ENABLE_FEATURE_INIT_SYSLOG) {
		/* Log the message to syslogd */
		if (where & L_LOG) {
			/* don't out "\r" */
			openlog(applet_name, 0, LOG_DAEMON);
			syslog(LOG_INFO, "init: %s", msg + 1);
			closelog();
		}
		msg[l++] = '\n';
		msg[l] = '\0';
	} else {
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
					where = L_CONSOLE;
				} else {
					close_on_exec_on(log_fd);
				}
			}
		}
		if (where & L_LOG) {
			full_write(log_fd, msg, l);
			if (log_fd == 2)
				return; /* don't print dup messages */
		}
	}

	if (where & L_CONSOLE) {
		/* Send console messages to console so people will see them. */
		full_write(STDERR_FILENO, msg, l);
	}
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
			xmove_fd(fd, 2);
		}
		messageD(L_LOG, "console='%s'", s);
	} else {
		/* Make sure fd 0,1,2 are not closed
		 * (so that they won't be used by future opens) */

		/* bb_sanitize_stdio(); - WRONG.
		 * It fails if "/dev/null" doesnt exist, and for init
		 * this is a real possibility! Open code it instead. */

		int fd = open(bb_dev_null, O_RDWR);
		if (fd < 0) {
			/* Give me _ANY_ open descriptor! */
			fd = xopen("/", O_RDONLY); /* we don't believe this can fail */
		}
		while ((unsigned)fd < 2)
			fd = dup(fd);
		if (fd > 2)
			close(fd);
	}

	s = getenv("TERM");
	if (ioctl(STDIN_FILENO, TIOCGSERIAL, &sr) == 0) {
		/* Force the TERM setting to vt102 for serial console
		 * if TERM is set to linux (the default) */
		if (!s || strcmp(s, "linux") == 0)
			putenv((char*)"TERM=vt102");
		if (!ENABLE_FEATURE_INIT_SYSLOG)
			log_console = NULL;
	} else if (!s)
		putenv((char*)"TERM=linux");
}

/* Set terminal settings to reasonable defaults.
 * NB: careful, we can be called after vfork! */
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

/* Open the new terminal device.
 * NB: careful, we can be called after vfork! */
static void open_stdio_to_tty(const char* tty_name, int exit_on_failure)
{
	/* empty tty_name means "use init's tty", else... */
	if (tty_name[0]) {
		int fd;
		close(0);
		/* fd can be only < 0 or 0: */
		fd = device_open(tty_name, O_RDWR);
		if (fd) {
			message(L_LOG | L_CONSOLE, "Can't open %s: %s",
				tty_name, strerror(errno));
			if (exit_on_failure)
				_exit(EXIT_FAILURE);
			if (ENABLE_DEBUG_INIT)
				_exit(2);
		/* NB: we don't reach this if we were called after vfork.
		 * Thus halt_reboot_pwoff() itself need not be vfork-safe. */
			halt_reboot_pwoff(SIGUSR1); /* halt the system */
		}
		dup2(0, 1);
		dup2(0, 2);
	}
	set_sane_term();
}

/* Wrapper around exec:
 * Takes string (max COMMAND_SIZE chars).
 * If chars like '>' detected, execs '[-]/bin/sh -c "exec ......."'.
 * Otherwise splits words on whitespace, deals with leading dash,
 * and uses plain exec().
 * NB: careful, we can be called after vfork!
 */
static void init_exec(const char *command)
{
	char *cmd[COMMAND_SIZE / 2];
	char buf[COMMAND_SIZE + 6];  /* COMMAND_SIZE+strlen("exec ")+1 */
	int dash = (command[0] == '-' /* maybe? && command[1] == '/' */);

	/* See if any special /bin/sh requiring characters are present */
	if (strpbrk(command, "~`!$^&*()=|\\{}[];\"'<>?") != NULL) {
		strcpy(buf, "exec ");
		strcpy(buf + 5, command + dash); /* excluding "-" */
		/* NB: LIBBB_DEFAULT_LOGIN_SHELL define has leading dash */
		cmd[0] = (char*)(LIBBB_DEFAULT_LOGIN_SHELL + !dash);
		cmd[1] = (char*)"-c";
		cmd[2] = buf;
		cmd[3] = NULL;
	} else {
		/* Convert command (char*) into cmd (char**, one word per string) */
		char *word, *next;
		int i = 0;
		next = strcpy(buf, command); /* including "-" */
		while ((word = strsep(&next, " \t")) != NULL) {
			if (*word != '\0') { /* not two spaces/tabs together? */
				cmd[i] = word;
				i++;
			}
		}
		cmd[i] = NULL;
	}
	/* If we saw leading "-", it is interactive shell.
	 * Try harder to give it a controlling tty.
	 * And skip "-" in actual exec call. */
	if (dash) {
		/* _Attempt_ to make stdin a controlling tty. */
		if (ENABLE_FEATURE_INIT_SCTTY)
			ioctl(STDIN_FILENO, TIOCSCTTY, 0 /*only try, don't steal*/);
	}
	BB_EXECVP(cmd[0] + dash, cmd);
	message(L_LOG | L_CONSOLE, "Cannot run '%s': %s",
			cmd[0], strerror(errno));
	/* returns if execvp fails */
}

/* Used only by run_actions */
static pid_t run(const struct init_action *a)
{
	pid_t pid;
	sigset_t nmask, omask;

	/* Block sigchild while forking (why?) */
	sigemptyset(&nmask);
	sigaddset(&nmask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &nmask, &omask);
	if (BB_MMU && (a->action_type & ASKFIRST))
		pid = fork();
	else
		pid = vfork();
	sigprocmask(SIG_SETMASK, &omask, NULL);

	if (pid < 0)
		message(L_LOG | L_CONSOLE, "Can't fork");
	if (pid)
		return pid;

	/* Child */

	/* Reset signal handlers that were set by the parent process */
	bb_signals(0
		+ (1 << SIGUSR1)
		+ (1 << SIGUSR2)
		+ (1 << SIGINT)
		+ (1 << SIGTERM)
		+ (1 << SIGHUP)
		+ (1 << SIGQUIT)
		+ (1 << SIGCONT)
		+ (1 << SIGSTOP)
		+ (1 << SIGTSTP)
		, SIG_DFL);

	/* Create a new session and make ourself the process
	 * group leader */
	setsid();

	/* Open the new terminal device */
	open_stdio_to_tty(a->terminal, 1 /* - exit if open fails */);

// NB: do not enable unless you change vfork to fork above
#ifdef BUT_RUN_ACTIONS_ALREADY_DOES_WAITING
	/* If the init Action requires us to wait, then force the
	 * supplied terminal to be the controlling tty. */
	if (a->action_type & (SYSINIT | WAIT | CTRLALTDEL | SHUTDOWN | RESTART)) {
		/* Now fork off another process to just hang around */
		pid = fork();
		if (pid < 0) {
			message(L_LOG | L_CONSOLE, "Can't fork");
			_exit(EXIT_FAILURE);
		}

		if (pid > 0) {
			/* Parent - wait till the child is done */
			bb_signals(0
				+ (1 << SIGINT)
				+ (1 << SIGTSTP)
				+ (1 << SIGQUIT)
				, SIG_IGN);
			signal(SIGCHLD, SIG_DFL);

			waitfor(pid);
			/* See if stealing the controlling tty back is necessary */
			if (tcgetpgrp(0) != getpid())
				_exit(EXIT_SUCCESS);

			/* Use a temporary process to steal the controlling tty. */
			pid = fork();
			if (pid < 0) {
				message(L_LOG | L_CONSOLE, "Can't fork");
				_exit(EXIT_FAILURE);
			}
			if (pid == 0) {
				setsid();
				ioctl(0, TIOCSCTTY, 1);
				_exit(EXIT_SUCCESS);
			}
			waitfor(pid);
			_exit(EXIT_SUCCESS);
		}

		/* Child - fall though to actually execute things */
	}
#endif

	/* NB: on NOMMU we can't wait for input in child, so
	 * "askfirst" will work the same as "respawn". */
	if (BB_MMU && (a->action_type & ASKFIRST)) {
		static const char press_enter[] ALIGN1 =
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
				a->command, getpid(), a->terminal);
		full_write(STDOUT_FILENO, press_enter, sizeof(press_enter) - 1);
		while (safe_read(STDIN_FILENO, &c, 1) == 1 && c != '\n')
			continue;
	}

	if (ENABLE_FEATURE_INIT_COREDUMPS) {
		struct stat sb;
		if (stat(CORE_ENABLE_FLAG_FILE, &sb) == 0) {
			struct rlimit limit;
			limit.rlim_cur = RLIM_INFINITY;
			limit.rlim_max = RLIM_INFINITY;
			setrlimit(RLIMIT_CORE, &limit);
		}
	}

	/* Log the process name and args */
	message(L_LOG, "starting pid %d, tty '%s': '%s'",
			  getpid(), a->terminal, a->command);

	/* Now run it.  The new program will take over this PID,
	 * so nothing further in init.c should be run. */
	init_exec(a->command);
	/* We're still here?  Some error happened. */
	_exit(-1);
}

/* Run all commands of a particular type */
static void run_actions(int action_type)
{
	struct init_action *a, *tmp;

	for (a = init_action_list; a; a = tmp) {
		tmp = a->next;
		if (a->action_type & action_type) {
			// Pointless: run() will error out if open of device fails.
			///* a->terminal of "" means "init's console" */
			//if (a->terminal[0] && access(a->terminal, R_OK | W_OK)) {
			//	//message(L_LOG | L_CONSOLE, "Device %s cannot be opened in RW mode", a->terminal /*, strerror(errno)*/);
			//	delete_init_action(a);
			//} else
			if (a->action_type & (SYSINIT | WAIT | CTRLALTDEL | SHUTDOWN | RESTART)) {
				waitfor(run(a));
				delete_init_action(a);
			} else if (a->action_type & ONCE) {
				run(a);
				delete_init_action(a);
			} else if (a->action_type & (RESPAWN | ASKFIRST)) {
				/* Only run stuff with pid==0.  If they have
				 * a pid, that means it is still running */
				if (a->pid == 0) {
					a->pid = run(a);
				}
			}
		}
	}
}

static void init_reboot(unsigned long magic)
{
	pid_t pid;
	/* We have to fork here, since the kernel calls do_exit(EXIT_SUCCESS) in
	 * linux/kernel/sys.c, which can cause the machine to panic when
	 * the init process is killed.... */
	pid = vfork();
	if (pid == 0) { /* child */
		reboot(magic);
		_exit(EXIT_SUCCESS);
	}
	waitfor(pid);
}

static void kill_all_processes(void)
{
	/* run everything to be run at "shutdown".  This is done _prior_
	 * to killing everything, in case people wish to use scripts to
	 * shut things down gracefully... */
	run_actions(SHUTDOWN);

	/* first disable all our signals */
	sigprocmask_allsigs(SIG_BLOCK);

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

static void halt_reboot_pwoff(int sig)
{
	const char *m;
	int rb;

	kill_all_processes();

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

/* Handler for QUIT - exec "restart" action,
 * else (no such action defined) do nothing */
static void exec_restart_action(int sig ATTRIBUTE_UNUSED)
{
	struct init_action *a;

	for (a = init_action_list; a; a = a->next) {
		if (a->action_type & RESTART) {
			kill_all_processes();

			/* unblock all signals (blocked in kill_all_processes()) */
			sigprocmask_allsigs(SIG_UNBLOCK);

			/* Open the new terminal device */
			open_stdio_to_tty(a->terminal, 0 /* - halt if open fails */);

			messageD(L_CONSOLE | L_LOG, "Trying to re-exec %s", a->command);
			init_exec(a->command);
			sleep(2);
			init_reboot(RB_HALT_SYSTEM);
			loop_forever();
		}
	}
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

	errno = saved_errno;
}

/* The SIGCONT handler */
static void cont_handler(int sig ATTRIBUTE_UNUSED)
{
	got_cont = 1;
}

static void new_init_action(uint8_t action_type, const char *command, const char *cons)
{
	struct init_action *a, *last;

// Why?
//	if (strcmp(cons, bb_dev_null) == 0 && (action & ASKFIRST))
//		return;

	/* Append to the end of the list */
	for (a = last = init_action_list; a; a = a->next) {
		/* don't enter action if it's already in the list,
		 * but do overwrite existing actions */
		if ((strcmp(a->command, command) == 0)
		 && (strcmp(a->terminal, cons) == 0)
		) {
			a->action_type = action_type;
			return;
		}
		last = a;
	}

	a = xzalloc(sizeof(*a));
	if (last) {
		last->next = a;
	} else {
		init_action_list = a;
	}
	a->action_type = action_type;
	safe_strncpy(a->command, command, sizeof(a->command));
	safe_strncpy(a->terminal, cons, sizeof(a->terminal));
	messageD(L_LOG | L_CONSOLE, "command='%s' action=%d tty='%s'\n",
		a->command, a->action_type, a->terminal);
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
	FILE *file;
	char buf[COMMAND_SIZE];

	if (ENABLE_FEATURE_USE_INITTAB)
		file = fopen(INITTAB, "r");
	else
		file = NULL;

	/* No inittab file -- set up some default behavior */
	if (file == NULL) {
		/* Reboot on Ctrl-Alt-Del */
		new_init_action(CTRLALTDEL, "reboot", "");
		/* Umount all filesystems on halt/reboot */
		new_init_action(SHUTDOWN, "umount -a -r", "");
		/* Swapoff on halt/reboot */
		if (ENABLE_SWAPONOFF)
			new_init_action(SHUTDOWN, "swapoff -a", "");
		/* Prepare to restart init when a QUIT is received */
		new_init_action(RESTART, "init", "");
		/* Askfirst shell on tty1-4 */
		new_init_action(ASKFIRST, bb_default_login_shell, "");
		new_init_action(ASKFIRST, bb_default_login_shell, VC_2);
		new_init_action(ASKFIRST, bb_default_login_shell, VC_3);
		new_init_action(ASKFIRST, bb_default_login_shell, VC_4);
		/* sysinit */
		new_init_action(SYSINIT, INIT_SCRIPT, "");

		return;
	}

	while (fgets(buf, COMMAND_SIZE, file) != NULL) {
		static const char actions[] =
			STR_SYSINIT    "sysinit\0"
			STR_RESPAWN    "respawn\0"
			STR_ASKFIRST   "askfirst\0"
			STR_WAIT       "wait\0"
			STR_ONCE       "once\0"
			STR_CTRLALTDEL "ctrlaltdel\0"
			STR_SHUTDOWN   "shutdown\0"
			STR_RESTART    "restart\0"
		;
		char tmpConsole[CONSOLE_NAME_SIZE];
		char *id, *runlev, *action, *command;
		const char *a;

		/* Skip leading spaces */
		id = skip_whitespace(buf);
		/* Trim the trailing '\n' */
		*strchrnul(id, '\n') = '\0';
		/* Skip the line if it is a comment */
		if (*id == '#' || *id == '\0')
			continue;

		/* Line is: "id:runlevel_ignored:action:command" */
		runlev = strchr(id, ':');
		if (runlev == NULL /*|| runlev[1] == '\0' - not needed */)
			goto bad_entry;
		action = strchr(runlev + 1, ':');
		if (action == NULL /*|| action[1] == '\0' - not needed */)
			goto bad_entry;
		command = strchr(action + 1, ':');
		if (command == NULL || command[1] == '\0')
			goto bad_entry;

		*command = '\0'; /* action => ":action\0" now */
		for (a = actions; a[0]; a += strlen(a) + 1) {
			if (strcmp(a + 1, action + 1) == 0) {
				*runlev = '\0';
				if (*id != '\0') {
					if (strncmp(id, "/dev/", 5) == 0)
						id += 5;
					strcpy(tmpConsole, "/dev/");
					safe_strncpy(tmpConsole + 5, id,
						sizeof(tmpConsole) - 5);
					id = tmpConsole;
				}
				new_init_action((uint8_t)a[0], command + 1, id);
				goto next_line;
			}
		}
		*command = ':';
		/* Choke on an unknown action */
 bad_entry:
		message(L_LOG | L_CONSOLE, "Bad inittab entry: %s", id);
 next_line: ;
	}
	fclose(file);
}

#if ENABLE_FEATURE_USE_INITTAB
static void reload_signal(int sig ATTRIBUTE_UNUSED)
{
	struct init_action *a, *tmp;

	message(L_LOG, "reloading /etc/inittab");

	/* disable old entrys */
	for (a = init_action_list; a; a = a->next) {
		a->action_type = ONCE;
	}

	parse_inittab();

	if (ENABLE_FEATURE_KILL_REMOVED) {
		/* Be nice and send SIGTERM first */
		for (a = init_action_list; a; a = a->next) {
			pid_t pid = a->pid;
			if ((a->action_type & ONCE) && pid != 0) {
				kill(pid, SIGTERM);
			}
		}
#if CONFIG_FEATURE_KILL_DELAY
		/* NB: parent will wait in NOMMU case */
		if ((BB_MMU ? fork() : vfork()) == 0) { /* child */
			sleep(CONFIG_FEATURE_KILL_DELAY);
			for (a = init_action_list; a; a = a->next) {
				pid_t pid = a->pid;
				if ((a->action_type & ONCE) && pid != 0) {
					kill(pid, SIGKILL);
				}
			}
			_exit(EXIT_SUCCESS);
		}
#endif
	}

	/* remove unused entrys */
	for (a = init_action_list; a; a = tmp) {
		tmp = a->next;
		if ((a->action_type & (ONCE | SYSINIT | WAIT)) && a->pid == 0) {
			delete_init_action(a);
		}
	}
	run_actions(RESPAWN | ASKFIRST);
}
#endif

int init_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int init_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	struct init_action *a;
	pid_t wpid;

	die_sleep = 30 * 24*60*60; /* if xmalloc will ever die... */

	if (argv[1] && !strcmp(argv[1], "-q")) {
		return kill(1, SIGHUP);
	}

	if (!ENABLE_DEBUG_INIT) {
		/* Expect to be invoked as init with PID=1 or be invoked as linuxrc */
		if (getpid() != 1
		 && (!ENABLE_FEATURE_INITRD || !strstr(applet_name, "linuxrc"))
		) {
			bb_show_usage();
		}
		/* Set up sig handlers  -- be sure to
		 * clear all of these in run() */
		signal(SIGQUIT, exec_restart_action);
		bb_signals(0
			+ (1 << SIGUSR1)  /* halt */
			+ (1 << SIGUSR2)  /* poweroff */
			+ (1 << SIGTERM)  /* reboot */
			, halt_reboot_pwoff);
		signal(SIGINT, ctrlaltdel_signal);
		signal(SIGCONT, cont_handler);
		bb_signals(0
			+ (1 << SIGSTOP)
			+ (1 << SIGTSTP)
			, stop_handler);

		/* Turn off rebooting via CTL-ALT-DEL -- we get a
		 * SIGINT on CAD so we can shut things down gracefully... */
		init_reboot(RB_DISABLE_CAD);
	}

	/* Figure out where the default console should be */
	console_init();
	set_sane_term();
	chdir("/");
	setsid();
	{
		const char *const *e;
		/* Make sure environs is set to something sane */
		for (e = environment; *e; e++)
			putenv((char *) *e);
	}

	if (argv[1]) setenv("RUNLEVEL", argv[1], 1);

	/* Hello world */
	message(MAYBE_CONSOLE | L_LOG, "init started: %s", bb_banner);

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
	if (argv[1]
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

		putenv((char*)"SELINUX_INIT=YES");
		if (selinux_init_load_policy(&enforce) == 0) {
			BB_EXECVP(argv[0], argv);
		} else if (enforce > 0) {
			/* SELinux in enforcing mode but load_policy failed */
			message(L_CONSOLE, "Cannot load SELinux Policy. "
				"Machine is in enforcing mode. Halting now.");
			exit(EXIT_FAILURE);
		}
	}
#endif /* CONFIG_SELINUX */

	/* Make the command line just say "init"  - thats all, nothing else */
	strncpy(argv[0], "init", strlen(argv[0]));
	/* Wipe argv[1]-argv[N] so they don't clutter the ps listing */
	while (*++argv)
		memset(*argv, 0, strlen(*argv));

	/* Now run everything that needs to be run */

	/* First run the sysinit command */
	run_actions(SYSINIT);

	/* Next run anything that wants to block */
	run_actions(WAIT);

	/* Next run anything to be run only once */
	run_actions(ONCE);

	/* Redefine SIGHUP to reread /etc/inittab */
#if ENABLE_FEATURE_USE_INITTAB
	signal(SIGHUP, reload_signal);
#else
	signal(SIGHUP, SIG_IGN);
#endif

	/* Now run the looping stuff for the rest of forever */
	while (1) {
		/* run the respawn/askfirst stuff */
		run_actions(RESPAWN | ASKFIRST);

		/* Don't consume all CPU time -- sleep a bit */
		sleep(1);

		/* Wait for any child process to exit */
		wpid = wait(NULL);
		while (wpid > 0) {
			/* Find out who died and clean up their corpse */
			for (a = init_action_list; a; a = a->next) {
				if (a->pid == wpid) {
					/* Set the pid to 0 so that the process gets
					 * restarted by run_actions() */
					a->pid = 0;
					message(L_LOG, "process '%s' (pid %d) exited. "
							"Scheduling for restart.",
							a->command, wpid);
				}
			}
			/* see if anyone else is waiting to be reaped */
			wpid = wait_any_nohang(NULL);
		}
	}
}
