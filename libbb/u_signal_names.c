/* vi: set sw=4 ts=4: */
/*
 * Signal name/number conversion routines.
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

static const struct signal_name {
	int number;
	char name[5];
} signals[] = {
	// SUSv3 says kill must support these, and specifies the numerical values,
	// http://www.opengroup.org/onlinepubs/009695399/utilities/kill.html
	{0, "0"}, {1, "HUP"}, {2, "INT"}, {3, "QUIT"}, {6, "ABRT"}, {9, "KILL"},
	{14, "ALRM"}, {15, "TERM"},
	// And Posix adds the following:
	{SIGILL, "ILL"}, {SIGTRAP, "TRAP"}, {SIGFPE, "FPE"}, {SIGUSR1, "USR1"},
	{SIGSEGV, "SEGV"}, {SIGUSR2, "USR2"}, {SIGPIPE, "PIPE"}, {SIGCHLD, "CHLD"},
	{SIGCONT, "CONT"}, {SIGSTOP, "STOP"}, {SIGTSTP, "TSTP"}, {SIGTTIN, "TTIN"},
	{SIGTTOU, "TTOU"}
};

// Convert signal name to number.

int get_signum(const char *name)
{
	int i;

	i = atoi(name);
	if (i) return i;
	for (i = 0; i < sizeof(signals) / sizeof(struct signal_name); i++)
		if (!strcasecmp(signals[i].name, name) ||
			(!strncasecmp(signals[i].name, "SIG", 3)
				 && !strcasecmp(signals[i].name+3, signals[i].name)))
					return signals[i].number;
	return -1;
}

// Convert signal number to name

const char *get_signame(int number)
{
	int i;
	static char buf[8];

	for (i=0; i < sizeof(signals) / sizeof(struct signal_name); i++) {
		if (number == signals[i].number) {
			return signals[i].name;
		}
	}

	itoa_to_buf(number, buf, 8);
	return buf;
}
