/* vi: set sw=4 ts=4: */
/*
 * Mini init implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Adjusted by so many folks, it's impossible to keep track.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

/* Turn this on to disable all the dangerous
   rebooting stuff when debugging.
#define DEBUG_INIT
*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <paths.h>
#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <limits.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include "busybox.h"

#include "init_shared.h"


#ifdef CONFIG_SYSLOGD
# include <sys/syslog.h>
#endif


#if defined(__UCLIBC__) && !defined(__ARCH_HAS_MMU__)
#define fork	vfork
#endif

#define INIT_BUFFS_SIZE 256

/* From <linux/vt.h> */
struct vt_stat {
	unsigned short v_active;	/* active vt */
	unsigned short v_signal;	/* signal to send */
	unsigned short v_state;	/* vt bitmask */
};
static const int VT_GETSTATE = 0x5603;	/* get global vt state info */

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
};


#ifndef _PATH_STDPATH
#define _PATH_STDPATH	"/usr/bin:/bin:/usr/sbin:/sbin"
#endif

#if defined CONFIG_FEATURE_INIT_COREDUMPS
/*
 * When a file named CORE_ENABLE_FLAG_FILE exists, setrlimit is called
 * before processes are spawned to set core file size as unlimited.
 * This is for debugging only.  Don't use this is production, unless
 * you want core dumps lying about....
 */
#define CORE_ENABLE_FLAG_FILE "/.init_enable_core"
#include <sys/resource.h>
#include <sys/time.h>
#endif

#define KERNEL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

#define INITTAB      "/etc/inittab"	/* inittab file location */
#ifndef INIT_SCRIPT
#define INIT_SCRIPT  "/etc/init.d/rcS"	/* Default sysinit script. */
#endif

#define MAXENV	16		/* Number of env. vars */

#define CONSOLE_BUFF_SIZE 32

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
	pid_t pid;
	char command[INIT_BUFFS_SIZE];
	char terminal[CONSOLE_BUFF_SIZE];
	struct init_action *next;
	int action;
};

/* Static variables */
static struct init_action *init_action_list = NULL;
static char console[CONSOLE_BUFF_SIZE] = _PATH_CONSOLE;

#ifndef CONFIG_SYSLOGD
static char *log = VC_5;
#endif
static sig_atomic_t got_cont = 0;
static const int LOG = 0x1;
static const int CONSOLE = 0x2;

#if defined CONFIG_FEATURE_EXTRA_QUIET
static const int MAYBE_CONSOLE = 0x0;
#else
#define MAYBE_CONSOLE	CONSOLE
#endif
#ifndef RB_HALT_SYSTEM
static const int RB_HALT_SYSTEM = 0xcdef0123;
static const int RB_ENABLE_CAD = 0x89abcdef;
static const int RB_DISABLE_CAD = 0;

#define RB_POWER_OFF    0x4321fedc
static const int RB_AUTOBOOT = 0x01234567;
#endif

static const char * const environment[] = {
	"HOME=/",
	"PATH=" _PATH_STDPATH,
	"SHELL=/bin/sh",
	"USER=root",
	NULL
};

/* Function prototypes */
static void delete_init_action(struct init_action *a);
static int waitfor(const struct init_action *a);
static void halt_signal(int sig);


static void loop_forever(void)
{
	while (1)
		sleep(1);
}

/* Print a message to the specified device.
 * Device may be bitwise-or'd from LOG | CONSOLE */
#ifndef DEBUG_INIT
static inline void messageD(int device, const char *fmt, ...)
{
}
#else
#define messageD message
#endif
static void message(int device, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));
static void message(int device, const char *fmt, ...)
{
	va_list arguments;
	int l;
	char msg[1024];
#ifndef CONFIG_SYSLOGD
	static int log_fd = -1;
#endif

	msg[0] = '\r';
		va_start(arguments, fmt);
	l = vsnprintf(msg + 1, sizeof(msg) - 2, fmt, arguments) + 1;
		va_end(arguments);

#ifdef CONFIG_SYSLOGD
	/* Log the message to syslogd */
	if (device & LOG) {
		/* don`t out "\r\n" */
		syslog_msg(LOG_DAEMON, LOG_INFO, msg + 1);
	}

	msg[l++] = '\n';
	msg[l] = 0;
#else

	msg[l++] = '\n';
	msg[l] = 0;
	/* Take full control of the log tty, and never close it.
	 * It's mine, all mine!  Muhahahaha! */
	if (log_fd < 0) {
		if ((log_fd = device_open(log, O_RDWR | O_NDELAY | O_NOCTTY)) < 0) {
			log_fd = -2;
			bb_error_msg("Bummer, can't write to log on %s!", log);
			device = CONSOLE;
		} else {
			fcntl(log_fd, F_SETFD, FD_CLOEXEC);
		}
	}
	if ((device & LOG) && (log_fd >= 0)) {
		bb_full_write(log_fd, msg, l);
	}
#endif

	if (device & CONSOLE) {
		int fd = device_open(_PATH_CONSOLE,
					O_WRONLY | O_NOCTTY | O_NDELAY);
		/* Always send console messages to /dev/console so people will see them. */
		if (fd >= 0) {
			bb_full_write(fd, msg, l);
			close(fd);
#ifdef DEBUG_INIT
		/* all descriptors may be closed */
		} else {
			bb_error_msg("Bummer, can't print: ");
			va_start(arguments, fmt);
			vfprintf(stderr, fmt, arguments);
			va_end(arguments);
#endif
		}
	}
}

/* Set terminal settings to reasonable defaults */
static void set_term(int fd)
{
	struct termios tty;

	tcgetattr(fd, &tty);

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

	tcsetattr(fd, TCSANOW, &tty);
}

/* How much memory does this machine have?
   Units are kBytes to avoid overflow on 4GB machines */
static unsigned int check_free_memory(void)
{
	struct sysinfo info;
	unsigned int result, u, s = 10;

	if (sysinfo(&info) != 0) {
		bb_perror_msg("Error checking free memory");
		return -1;
	}

	/* Kernels 2.0.x and 2.2.x return info.mem_unit==0 with values in bytes.
	 * Kernels 2.4.0 return info.mem_unit in bytes. */
	u = info.mem_unit;
	if (u == 0)
		u = 1;
	while ((u & 1) == 0 && s > 0) {
		u >>= 1;
		s--;
	}
	result = (info.totalram >> s) + (info.totalswap >> s);
	if (((unsigned long long)result * (unsigned long long)u) > UINT_MAX) {
		return(UINT_MAX);
	} else {
		return(result * u);
	}
}

static void console_init(void)
{
	int fd;
	int tried = 0;
	struct vt_stat vt;
	struct serial_struct sr;
	char *s;

	if ((s = getenv("CONSOLE")) != NULL || (s = getenv("console")) != NULL) {
		safe_strncpy(console, s, sizeof(console));
#if #cpu(sparc)
	/* sparc kernel supports console=tty[ab] parameter which is also
	 * passed to init, so catch it here */
		/* remap tty[ab] to /dev/ttyS[01] */
		if (strcmp(s, "ttya") == 0)
			safe_strncpy(console, SC_0, sizeof(console));
		else if (strcmp(s, "ttyb") == 0)
			safe_strncpy(console, SC_1, sizeof(console));
#endif
	} else {
		/* 2.2 kernels: identify the real console backend and try to use it */
		if (ioctl(0, TIOCGSERIAL, &sr) == 0) {
			/* this is a serial console */
			snprintf(console, sizeof(console) - 1, SC_FORMAT, sr.line);
		} else if (ioctl(0, VT_GETSTATE, &vt) == 0) {
			/* this is linux virtual tty */
			snprintf(console, sizeof(console) - 1, VC_FORMAT, vt.v_active);
		} else {
			safe_strncpy(console, _PATH_CONSOLE, sizeof(console));
			tried++;
		}
	}

	while ((fd = open(console, O_RDONLY | O_NONBLOCK)) < 0 && tried < 2) {
		/* Can't open selected console -- try
			logical system console and VT_MASTER */
		safe_strncpy(console, (tried == 0 ? _PATH_CONSOLE : CURRENT_VC),
							sizeof(console));
		tried++;
	}
	if (fd < 0) {
		/* Perhaps we should panic here? */
#ifndef CONFIG_SYSLOGD
		log =
#endif
		safe_strncpy(console, "/dev/null", sizeof(console));
	} else {
		s = getenv("TERM");
		/* check for serial console */
		if (ioctl(fd, TIOCGSERIAL, &sr) == 0) {
			/* Force the TERM setting to vt102 for serial console --
			 * if TERM is set to linux (the default) */
			if (s == NULL || strcmp(s, "linux") == 0)
				putenv("TERM=vt102");
#ifndef CONFIG_SYSLOGD
			log = console;
#endif
		} else {
			if (s == NULL)
				putenv("TERM=linux");
		}
		close(fd);
	}
	messageD(LOG, "console=%s", console);
}

static void fixup_argv(int argc, char **argv, char *new_argv0)
{
	int len;

	/* Fix up argv[0] to be certain we claim to be init */
	len = strlen(argv[0]);
	memset(argv[0], 0, len);
	safe_strncpy(argv[0], new_argv0, len + 1);

	/* Wipe argv[1]-argv[N] so they don't clutter the ps listing */
	len = 1;
	while (argc > len) {
		memset(argv[len], 0, strlen(argv[len]));
		len++;
	}
}

static pid_t run(const struct init_action *a)
{
	struct stat sb;
	int i, junk;
	pid_t pid, pgrp, tmp_pid;
	char *s, *tmpCmd, *cmd[INIT_BUFFS_SIZE], *cmdpath;
	char buf[INIT_BUFFS_SIZE + 6];	/* INIT_BUFFS_SIZE+strlen("exec ")+1 */
	sigset_t nmask, omask;
	static const char press_enter[] =
#ifdef CUSTOMIZED_BANNER
#include CUSTOMIZED_BANNER
#endif
		"\nPlease press Enter to activate this console. ";

	/* Block sigchild while forking.  */
	sigemptyset(&nmask);
	sigaddset(&nmask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &nmask, &omask);

	if ((pid = fork()) == 0) {
		/* Clean up */
		close(0);
		close(1);
		close(2);
		sigprocmask(SIG_SETMASK, &omask, NULL);

		/* Reset signal handlers that were set by the parent process */
		signal(SIGUSR1, SIG_DFL);
		signal(SIGUSR2, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		signal(SIGHUP, SIG_DFL);
		signal(SIGCONT, SIG_DFL);
		signal(SIGSTOP, SIG_DFL);
		signal(SIGTSTP, SIG_DFL);

		/* Create a new session and make ourself the process
		 * group leader */
		setsid();

		/* Open the new terminal device */
		if ((device_open(a->terminal, O_RDWR)) < 0) {
			if (stat(a->terminal, &sb) != 0) {
				message(LOG | CONSOLE, "device '%s' does not exist.",
						a->terminal);
				_exit(1);
			}
			message(LOG | CONSOLE, "Bummer, can't open %s", a->terminal);
			_exit(1);
		}

		/* Make sure the terminal will act fairly normal for us */
		set_term(0);
		/* Setup stdout, stderr for the new process so
		 * they point to the supplied terminal */
		dup(0);
		dup(0);

		/* If the init Action requires us to wait, then force the
		 * supplied terminal to be the controlling tty. */
		if (a->action & (SYSINIT | WAIT | CTRLALTDEL | SHUTDOWN | RESTART)) {

			/* Now fork off another process to just hang around */
			if ((pid = fork()) < 0) {
				message(LOG | CONSOLE, "Can't fork!");
				_exit(1);
			}

			if (pid > 0) {

				/* We are the parent -- wait till the child is done */
				signal(SIGINT, SIG_IGN);
				signal(SIGTSTP, SIG_IGN);
				signal(SIGQUIT, SIG_IGN);
				signal(SIGCHLD, SIG_DFL);

				/* Wait for child to exit */
				while ((tmp_pid = waitpid(pid, &junk, 0)) != pid) {
					if (tmp_pid == -1 && errno == ECHILD) {
						break;
					}
					/* FIXME handle other errors */
                }

				/* See if stealing the controlling tty back is necessary */
				pgrp = tcgetpgrp(0);
				if (pgrp != getpid())
					_exit(0);

				/* Use a temporary process to steal the controlling tty. */
				if ((pid = fork()) < 0) {
					message(LOG | CONSOLE, "Can't fork!");
					_exit(1);
				}
				if (pid == 0) {
					setsid();
					ioctl(0, TIOCSCTTY, 1);
					_exit(0);
				}
				while ((tmp_pid = waitpid(pid, &junk, 0)) != pid) {
					if (tmp_pid < 0 && errno == ECHILD)
						break;
				}
				_exit(0);
			}

			/* Now fall though to actually execute things */
		}

		/* See if any special /bin/sh requiring characters are present */
		if (strpbrk(a->command, "~`!$^&*()=|\\{}[];\"'<>?") != NULL) {
			cmd[0] = (char *)DEFAULT_SHELL;
			cmd[1] = "-c";
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
		   Interactive shells want to see a dash in argv[0].  This
		   typically is handled by login, argv will be setup this
		   way if a dash appears at the front of the command path
		   (like "-/bin/sh").
		 */

		if (*cmdpath == '-') {

			/* skip over the dash */
			++cmdpath;

			/* find the last component in the command pathname */
			s = bb_get_last_path_component(cmdpath);

			/* make a new argv[0] */
			if ((cmd[0] = malloc(strlen(s) + 2)) == NULL) {
				message(LOG | CONSOLE, bb_msg_memory_exhausted);
				cmd[0] = cmdpath;
			} else {
				cmd[0][0] = '-';
				strcpy(cmd[0] + 1, s);
			}
		}

		if (a->action & ASKFIRST) {
			char c;
			/*
			 * Save memory by not exec-ing anything large (like a shell)
			 * before the user wants it. This is critical if swap is not
			 * enabled and the system has low memory. Generally this will
			 * be run on the second virtual console, and the first will
			 * be allowed to start a shell or whatever an init script
			 * specifies.
			 */
			messageD(LOG, "Waiting for enter to start '%s'"
						"(pid %d, terminal %s)\n",
					  cmdpath, getpid(), a->terminal);
			bb_full_write(1, press_enter, sizeof(press_enter) - 1);
			while(read(0, &c, 1) == 1 && c != '\n')
				;
		}

		/* Log the process name and args */
		message(LOG, "Starting pid %d, console %s: '%s'",
				  getpid(), a->terminal, cmdpath);

#if defined CONFIG_FEATURE_INIT_COREDUMPS
		if (stat(CORE_ENABLE_FLAG_FILE, &sb) == 0) {
			struct rlimit limit;

			limit.rlim_cur = RLIM_INFINITY;
			limit.rlim_max = RLIM_INFINITY;
			setrlimit(RLIMIT_CORE, &limit);
		}
#endif

		/* Now run it.  The new program will take over this PID,
		 * so nothing further in init.c should be run. */
		execv(cmdpath, cmd);

		/* We're still here?  Some error happened. */
		message(LOG | CONSOLE, "Bummer, could not run '%s': %m", cmdpath);
		_exit(-1);
	}
	sigprocmask(SIG_SETMASK, &omask, NULL);
	return pid;
}

static int waitfor(const struct init_action *a)
{
	int pid;
	int status, wpid;

	pid = run(a);
	while (1) {
		wpid = waitpid(pid,&status,0);
		if (wpid == pid)
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
			if (a->action & (SYSINIT | WAIT | CTRLALTDEL | SHUTDOWN | RESTART)) {
				waitfor(a);
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

#ifndef DEBUG_INIT
static void init_reboot(unsigned long magic)
{
	pid_t pid;
	/* We have to fork here, since the kernel calls do_exit(0) in
	 * linux/kernel/sys.c, which can cause the machine to panic when
	 * the init process is killed.... */
	if ((pid = fork()) == 0) {
		reboot(magic);
		_exit(0);
	}
	waitpid (pid, NULL, 0);
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
	sigaddset(&block_signals, SIGCHLD);
	sigaddset(&block_signals, SIGUSR1);
	sigaddset(&block_signals, SIGUSR2);
	sigaddset(&block_signals, SIGINT);
	sigaddset(&block_signals, SIGTERM);
	sigaddset(&block_signals, SIGCONT);
	sigaddset(&block_signals, SIGSTOP);
	sigaddset(&block_signals, SIGTSTP);
	sigprocmask(SIG_BLOCK, &block_signals, NULL);

	/* Allow Ctrl-Alt-Del to reboot system. */
	init_reboot(RB_ENABLE_CAD);

	message(CONSOLE | LOG, "The system is going down NOW !!");
	sync();

	/* Send signals to every process _except_ pid 1 */
	message(CONSOLE | LOG, "Sending SIGTERM to all processes.");
	kill(-1, SIGTERM);
	sleep(1);
	sync();

	message(CONSOLE | LOG, "Sending SIGKILL to all processes.");
	kill(-1, SIGKILL);
	sleep(1);

	sync();
}

static void exec_signal(int sig)
{
	struct init_action *a, *tmp;
	sigset_t unblock_signals;

	for (a = init_action_list; a; a = tmp) {
		tmp = a->next;
		if (a->action & RESTART) {
			struct stat sb;

			shutdown_system();

			/* unblock all signals, blocked in shutdown_system() */
			sigemptyset(&unblock_signals);
			sigaddset(&unblock_signals, SIGHUP);
			sigaddset(&unblock_signals, SIGCHLD);
			sigaddset(&unblock_signals, SIGUSR1);
			sigaddset(&unblock_signals, SIGUSR2);
			sigaddset(&unblock_signals, SIGINT);
			sigaddset(&unblock_signals, SIGTERM);
			sigaddset(&unblock_signals, SIGCONT);
			sigaddset(&unblock_signals, SIGSTOP);
			sigaddset(&unblock_signals, SIGTSTP);
			sigprocmask(SIG_UNBLOCK, &unblock_signals, NULL);

			/* Close whatever files are open. */
			close(0);
			close(1);
			close(2);

			/* Open the new terminal device */
			if ((device_open(a->terminal, O_RDWR)) < 0) {
				if (stat(a->terminal, &sb) != 0) {
					message(LOG | CONSOLE, "device '%s' does not exist.", a->terminal);
				} else {
					message(LOG | CONSOLE, "Bummer, can't open %s", a->terminal);
				}
				halt_signal(SIGUSR1);
			}

			/* Make sure the terminal will act fairly normal for us */
			set_term(0);
			/* Setup stdout, stderr on the supplied terminal */
			dup(0);
			dup(0);

			messageD(CONSOLE | LOG, "Trying to re-exec %s", a->command);
			execl(a->command, a->command, NULL);

			message(CONSOLE | LOG, "exec of '%s' failed: %m",
					a->command);
			sync();
			sleep(2);
			init_reboot(RB_HALT_SYSTEM);
			loop_forever();
		}
	}
}

static void halt_signal(int sig)
{
	shutdown_system();
	message(CONSOLE | LOG,
#if #cpu(s390)
			/* Seems the s390 console is Wierd(tm). */
			"The system is halted. You may reboot now."
#else
			"The system is halted. Press Reset or turn off power"
#endif
		);
	sync();

	/* allow time for last message to reach serial console */
	sleep(2);

	if (sig == SIGUSR2)
		init_reboot(RB_POWER_OFF);
	else
		init_reboot(RB_HALT_SYSTEM);

	loop_forever();
}

static void reboot_signal(int sig)
{
	shutdown_system();
	message(CONSOLE | LOG, "Please stand by while rebooting the system.");
	sync();

	/* allow time for last message to reach serial console */
	sleep(2);

	init_reboot(RB_AUTOBOOT);

	loop_forever();
}

static void ctrlaltdel_signal(int sig)
{
	run_actions(CTRLALTDEL);
}

/* The SIGSTOP & SIGTSTP handler */
static void stop_handler(int sig)
{
	int saved_errno = errno;

	got_cont = 0;
	while (!got_cont)
		pause();
	got_cont = 0;
	errno = saved_errno;
}

/* The SIGCONT handler */
static void cont_handler(int sig)
{
	got_cont = 1;
}

#endif							/* ! DEBUG_INIT */

static void new_init_action(int action, const char *command, const char *cons)
{
	struct init_action *new_action, *a;

	if (*cons == '\0')
		cons = console;

	/* do not run entries if console device is not available */
	if (access(cons, R_OK | W_OK))
		return;
	if (strcmp(cons, "/dev/null") == 0 && (action & ASKFIRST))
		return;

	new_action = calloc((size_t) (1), sizeof(struct init_action));
	if (!new_action) {
		message(LOG | CONSOLE, "Memory allocation failure");
		loop_forever();
	}

	/* Append to the end of the list */
	for (a = init_action_list; a && a->next; a = a->next) {
		/* don't enter action if it's already in the list */
		if ((strcmp(a->command, command) == 0) &&
		    (strcmp(a->terminal, cons) ==0)) {
			free(new_action);
			return;
		}
	}
	if (a) {
		a->next = new_action;
	} else {
		init_action_list = new_action;
	}
	strcpy(new_action->command, command);
	new_action->action = action;
	strcpy(new_action->terminal, cons);
#if 0   /* calloc zeroed always */
	new_action->pid = 0;
#endif
	messageD(LOG|CONSOLE, "command='%s' action='%d' terminal='%s'\n",
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

/* Make sure there is enough memory to do something useful. *
 * Calls "swapon -a" if needed so be sure /etc/fstab is present... */
static void check_memory(void)
{
	struct stat statBuf;

	if (check_free_memory() > 1000)
		return;

#if !defined(__UCLIBC__) || defined(__ARCH_HAS_MMU__)
	if (stat("/etc/fstab", &statBuf) == 0) {
		/* swapon -a requires /proc typically */
		new_init_action(SYSINIT, "/bin/mount -t proc proc /proc", "");
		/* Try to turn on swap */
		new_init_action(SYSINIT, "/sbin/swapon -a", "");
		run_actions(SYSINIT);   /* wait and removing */
		if (check_free_memory() < 1000)
			goto goodnight;
	} else
		goto goodnight;
	return;
#endif

  goodnight:
	message(CONSOLE, "Sorry, your computer does not have enough memory.");
	loop_forever();
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
#ifdef CONFIG_FEATURE_USE_INITTAB
	FILE *file;
	char buf[INIT_BUFFS_SIZE], lineAsRead[INIT_BUFFS_SIZE];
	char tmpConsole[CONSOLE_BUFF_SIZE];
	char *id, *runlev, *action, *command, *eol;
	const struct init_action_type *a = actions;


	file = fopen(INITTAB, "r");
	if (file == NULL) {
		/* No inittab file -- set up some default behavior */
#endif
		/* Reboot on Ctrl-Alt-Del */
		new_init_action(CTRLALTDEL, "/sbin/reboot", "");
		/* Umount all filesystems on halt/reboot */
		new_init_action(SHUTDOWN, "/bin/umount -a -r", "");
#if !defined(__UCLIBC__) || defined(__ARCH_HAS_MMU__)
		/* Swapoff on halt/reboot */
		new_init_action(SHUTDOWN, "/sbin/swapoff -a", "");
#endif
		/* Prepare to restart init when a HUP is received */
		new_init_action(RESTART, "/sbin/init", "");
		/* Askfirst shell on tty1-4 */
		new_init_action(ASKFIRST, bb_default_login_shell, "");
		new_init_action(ASKFIRST, bb_default_login_shell, VC_2);
		new_init_action(ASKFIRST, bb_default_login_shell, VC_3);
		new_init_action(ASKFIRST, bb_default_login_shell, VC_4);
		/* sysinit */
		new_init_action(SYSINIT, INIT_SCRIPT, "");

		return;
#ifdef CONFIG_FEATURE_USE_INITTAB
	}

	while (fgets(buf, INIT_BUFFS_SIZE, file) != NULL) {
		/* Skip leading spaces */
		for (id = buf; *id == ' ' || *id == '\t'; id++);

		/* Skip the line if it's a comment */
		if (*id == '#' || *id == '\n')
			continue;

		/* Trim the trailing \n */
		eol = strrchr(id, '\n');
		if (eol != NULL)
			*eol = '\0';

		/* Keep a copy around for posterity's sake (and error msgs) */
		strcpy(lineAsRead, buf);

		/* Separate the ID field from the runlevels */
		runlev = strchr(id, ':');
		if (runlev == NULL || *(runlev + 1) == '\0') {
			message(LOG | CONSOLE, "Bad inittab entry: %s", lineAsRead);
			continue;
		} else {
			*runlev = '\0';
			++runlev;
		}

		/* Separate the runlevels from the action */
		action = strchr(runlev, ':');
		if (action == NULL || *(action + 1) == '\0') {
			message(LOG | CONSOLE, "Bad inittab entry: %s", lineAsRead);
			continue;
		} else {
			*action = '\0';
			++action;
		}

		/* Separate the action from the command */
		command = strchr(action, ':');
		if (command == NULL || *(command + 1) == '\0') {
			message(LOG | CONSOLE, "Bad inittab entry: %s", lineAsRead);
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
						CONSOLE_BUFF_SIZE - 5);
					id = tmpConsole;
				}
				new_init_action(a->action, command, id);
				break;
			}
		}
		if (a->name == 0) {
			/* Choke on an unknown action */
			message(LOG | CONSOLE, "Bad inittab entry: %s", lineAsRead);
		}
	}
	fclose(file);
	return;
#endif							/* CONFIG_FEATURE_USE_INITTAB */
}

static void reload_signal(int sig)
{
        message(LOG, "Reloading /etc/inittab");
        parse_inittab();
	run_actions(RESPAWN);
        return;
}
                                                            
extern int init_main(int argc, char **argv)
{
	struct init_action *a;
	pid_t wpid;
	int status;

	if (argc > 1 && !strcmp(argv[1], "-q")) {
		return kill_init(SIGHUP);
	}
#ifndef DEBUG_INIT
	/* Expect to be invoked as init with PID=1 or be invoked as linuxrc */
	if (getpid() != 1
#ifdef CONFIG_FEATURE_INITRD
		&& strstr(bb_applet_name, "linuxrc") == NULL
#endif
		) {
		bb_show_usage();
	}
	/* Set up sig handlers  -- be sure to
	 * clear all of these in run() */
	signal(SIGHUP, exec_signal);
	signal(SIGUSR1, halt_signal);
	signal(SIGUSR2, halt_signal);
	signal(SIGINT, ctrlaltdel_signal);
	signal(SIGTERM, reboot_signal);
	signal(SIGCONT, cont_handler);
	signal(SIGSTOP, stop_handler);
	signal(SIGTSTP, stop_handler);

	/* Turn off rebooting via CTL-ALT-DEL -- we get a
	 * SIGINT on CAD so we can shut things down gracefully... */
	init_reboot(RB_DISABLE_CAD);
#endif

	/* Figure out where the default console should be */
	console_init();

	/* Close whatever files are open, and reset the console. */
	close(0);
	close(1);
	close(2);

	if (device_open(console, O_RDWR | O_NOCTTY) == 0) {
		set_term(0);
		close(0);
	}

	chdir("/");
	setsid();
	{
		const char * const *e;
		/* Make sure environs is set to something sane */
		for(e = environment; *e; e++)
			putenv((char *) *e);
	}
	/* Hello world */
	message(MAYBE_CONSOLE | LOG, "init started:  %s", bb_msg_full_version);

	/* Make sure there is enough memory to do something useful. */
	check_memory();

	/* Check if we are supposed to be in single user mode */
	if (argc > 1 && (!strcmp(argv[1], "single") ||
					 !strcmp(argv[1], "-s") || !strcmp(argv[1], "1"))) {
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

	/* Make the command line just say "init"  -- thats all, nothing else */
	fixup_argv(argc, argv, "init");

	/* Now run everything that needs to be run */

	/* First run the sysinit command */
	run_actions(SYSINIT);

	/* Next run anything that wants to block */
	run_actions(WAIT);

	/* Next run anything to be run only once */
	run_actions(ONCE);

	/* Redefine SIGHUP to reread /etc/inittab */
	signal(SIGHUP, reload_signal);

	/* Now run the looping stuff for the rest of forever */
	while (1) {
		/* run the respawn stuff */
		run_actions(RESPAWN);

		/* run the askfirst stuff */
		run_actions(ASKFIRST);

		/* Don't consume all CPU time -- sleep a bit */
		sleep(1);

		/* Wait for a child process to exit */
		wpid = wait(&status);
		while (wpid > 0) {
			/* Find out who died and clean up their corpse */
			for (a = init_action_list; a; a = a->next) {
				if (a->pid == wpid) {
					/* Set the pid to 0 so that the process gets
					 * restarted by run_actions() */
					a->pid = 0;
					message(LOG, "Process '%s' (pid %d) exited.  "
							"Scheduling it for restart.",
							a->command, wpid);
				}
			}
			/* see if anyone else is waiting to be reaped */
			wpid = waitpid (-1, &status, WNOHANG);
		}
	}
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
