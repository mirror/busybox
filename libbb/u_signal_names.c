/* vi: set sw=4 ts=4: */
/*
 * Signal name/number conversion routines.
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#define KILL_MAX_SIG 32

static const char signals[KILL_MAX_SIG][6] = {
	// SUSv3 says kill must support these, and specifies the numerical values,
	// http://www.opengroup.org/onlinepubs/009695399/utilities/kill.html
	// {0, "EXIT"}, {1, "HUP"}, {2, "INT"}, {3, "QUIT"},
	// {6, "ABRT"}, {9, "KILL"}, {14, "ALRM"}, {15, "TERM"}
	// And Posix adds the following:
	// {SIGILL, "ILL"}, {SIGTRAP, "TRAP"}, {SIGFPE, "FPE"}, {SIGUSR1, "USR1"},
	// {SIGSEGV, "SEGV"}, {SIGUSR2, "USR2"}, {SIGPIPE, "PIPE"}, {SIGCHLD, "CHLD"},
	// {SIGCONT, "CONT"}, {SIGSTOP, "STOP"}, {SIGTSTP, "TSTP"}, {SIGTTIN, "TTIN"},
	// {SIGTTOU, "TTOU"}

/* Believe it or not, but some arches have more than 32 SIGs!
 * HPPA: SIGSTKFLT == 36. We don't include those. */

/* NB: longest (6-char) names are NOT nul-terminated */
	[0] = "EXIT",
#if defined SIGHUP && SIGHUP < KILL_MAX_SIG
	[SIGHUP   ] = "HUP",
#endif
#if defined SIGINT && SIGINT < KILL_MAX_SIG
	[SIGINT   ] = "INT",
#endif
#if defined SIGQUIT && SIGQUIT < KILL_MAX_SIG
	[SIGQUIT  ] = "QUIT",
#endif
#if defined SIGILL && SIGILL < KILL_MAX_SIG
	[SIGILL   ] = "ILL",
#endif
#if defined SIGTRAP && SIGTRAP < KILL_MAX_SIG
	[SIGTRAP  ] = "TRAP",
#endif
#if defined SIGABRT && SIGABRT < KILL_MAX_SIG
	[SIGABRT  ] = "ABRT",
#endif
#if defined SIGBUS && SIGBUS < KILL_MAX_SIG
	[SIGBUS   ] = "BUS",
#endif
#if defined SIGFPE && SIGFPE < KILL_MAX_SIG
	[SIGFPE   ] = "FPE",
#endif
#if defined SIGKILL && SIGKILL < KILL_MAX_SIG
	[SIGKILL  ] = "KILL",
#endif
#if defined SIGUSR1 && SIGUSR1 < KILL_MAX_SIG
	[SIGUSR1  ] = "USR1",
#endif
#if defined SIGSEGV && SIGSEGV < KILL_MAX_SIG
	[SIGSEGV  ] = "SEGV",
#endif
#if defined SIGUSR2 && SIGUSR2 < KILL_MAX_SIG
	[SIGUSR2  ] = "USR2",
#endif
#if defined SIGPIPE && SIGPIPE < KILL_MAX_SIG
	[SIGPIPE  ] = "PIPE",
#endif
#if defined SIGALRM && SIGALRM < KILL_MAX_SIG
	[SIGALRM  ] = "ALRM",
#endif
#if defined SIGTERM && SIGTERM < KILL_MAX_SIG
	[SIGTERM  ] = "TERM",
#endif
#if defined SIGSTKFLT && SIGSTKFLT < KILL_MAX_SIG
	[SIGSTKFLT] = "STKFLT",
#endif
#if defined SIGCHLD && SIGCHLD < KILL_MAX_SIG
	[SIGCHLD  ] = "CHLD",
#endif
#if defined SIGCONT && SIGCONT < KILL_MAX_SIG
	[SIGCONT  ] = "CONT",
#endif
#if defined SIGSTOP && SIGSTOP < KILL_MAX_SIG
	[SIGSTOP  ] = "STOP",
#endif
#if defined SIGTSTP && SIGTSTP < KILL_MAX_SIG
	[SIGTSTP  ] = "TSTP",
#endif
#if defined SIGTTIN && SIGTTIN < KILL_MAX_SIG
	[SIGTTIN  ] = "TTIN",
#endif
#if defined SIGTTOU && SIGTTOU < KILL_MAX_SIG
	[SIGTTOU  ] = "TTOU",
#endif
#if defined SIGURG && SIGURG < KILL_MAX_SIG
	[SIGURG   ] = "URG",
#endif
#if defined SIGXCPU && SIGXCPU < KILL_MAX_SIG
	[SIGXCPU  ] = "XCPU",
#endif
#if defined SIGXFSZ && SIGXFSZ < KILL_MAX_SIG
	[SIGXFSZ  ] = "XFSZ",
#endif
#if defined SIGVTALRM && SIGVTALRM < KILL_MAX_SIG
	[SIGVTALRM] = "VTALRM",
#endif
#if defined SIGPROF && SIGPROF < KILL_MAX_SIG
	[SIGPROF  ] = "PROF",
#endif
#if defined SIGWINCH && SIGWINCH < KILL_MAX_SIG
	[SIGWINCH ] = "WINCH",
#endif
#if defined SIGPOLL && SIGPOLL < KILL_MAX_SIG
	[SIGPOLL  ] = "POLL",
#endif
#if defined SIGPWR && SIGPWR < KILL_MAX_SIG
	[SIGPWR   ] = "PWR",
#endif
#if defined SIGSYS && SIGSYS < KILL_MAX_SIG
	[SIGSYS   ] = "SYS",
#endif
};

// Convert signal name to number.

int get_signum(const char *name)
{
	int i;

	i = bb_strtou(name, NULL, 10);
	if (!errno)
		return i;
	if (strncasecmp(name, "SIG", 3) == 0)
		name += 3;
	if (strlen(name) > 6)
		return -1;
	for (i = 0; i < ARRAY_SIZE(signals); i++)
		if (strncasecmp(name, signals[i], 6) == 0)
			return i;

#if ENABLE_DESKTOP && (defined(SIGIOT) || defined(SIGIO))
	/* These are aliased to other names */
	if ((name[0] | 0x20) == 'i' && (name[1] | 0x20) == 'o') {
#if defined SIGIO
		if (!name[2])
			return SIGIO;
#endif
#if defined SIGIOT
		if ((name[2] | 0x20) == 't' && !name[3])
			return SIGIOT;
#endif
	}
#endif

	return -1;
}

// Convert signal number to name

const char *get_signame(int number)
{
	if ((unsigned)number < ARRAY_SIZE(signals)) {
		if (signals[number][0]) /* if it's not an empty str */
			return signals[number];
	}

	return itoa(number);
}


// Print the whole signal list

void print_signames(void)
{
	int signo;

	for (signo = 1; signo < ARRAY_SIZE(signals); signo++) {
		const char *name = signals[signo];
		if (name[0])
			puts(name);
	}
}
