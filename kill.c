#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

const char	kill_usage[] = "kill [-signal] process-id [process-id ...]\n";

struct signal_name {
	const char *	name;
	int				number;
};

const struct signal_name signames[] = {
	{ "HUP",	SIGHUP },
	{ "INT",	SIGINT },
	{ "QUIT",	SIGQUIT },
	{ "ILL",	SIGILL },
	{ "TRAP",	SIGTRAP },
	{ "ABRT",	SIGABRT },
#ifndef __alpha__
	{ "IOT",	SIGIOT },
#endif
#if defined(sparc) || defined(__alpha__)
	{ "EMT",        SIGEMT },
#else
	{ "BUS",	SIGBUS },
#endif
	{ "FPE",	SIGFPE },
	{ "KILL",	SIGKILL },
#if defined(sparc) || defined(__alpha__)
	{ "BUS",        SIGBUS },
#else
	{ "USR1",	SIGUSR1 },
#endif
	{ "SEGV",	SIGSEGV },
#if defined(sparc) || defined(__alpha__)
	{ "SYS",        SIGSYS },
#else
	{ "USR2",	SIGUSR2 },
#endif
	{ "PIPE",	SIGPIPE },
	{ "ALRM",	SIGALRM },
	{ "TERM",	SIGTERM },
#if defined(sparc) || defined(__alpha__)
	{ "URG",        SIGURG },
	{ "STOP",       SIGSTOP },
	{ "TSTP",       SIGTSTP },
	{ "CONT",       SIGCONT },
	{ "CHLD",       SIGCHLD },
	{ "TTIN",       SIGTTIN },
	{ "TTOU",       SIGTTOU },
	{ "IO",         SIGIO },
# ifndef __alpha__
	{ "POLL",       SIGIO },
# endif
	{ "XCPU",       SIGXCPU },
	{ "XFSZ",       SIGXFSZ },
	{ "VTALRM",     SIGVTALRM },
	{ "PROF",       SIGPROF },
	{ "WINCH",      SIGWINCH },
# ifdef __alpha__
	{ "INFO",       SIGINFO },
# else
	{ "LOST",       SIGLOST },
# endif
	{ "USR1",       SIGUSR1 },
	{ "USR2",       SIGUSR2 },
#else
	{ "STKFLT",	SIGSTKFLT },
	{ "CHLD",	SIGCHLD },
	{ "CONT",	SIGCONT },
	{ "STOP",	SIGSTOP },
	{ "TSTP",	SIGTSTP },
	{ "TTIN",	SIGTTIN },
	{ "TTOU",	SIGTTOU },
	{ "URG",	SIGURG },
	{ "XCPU",	SIGXCPU },
	{ "XFSZ",	SIGXFSZ },
	{ "VTALRM",	SIGVTALRM },
	{ "PROF",	SIGPROF },
	{ "WINCH",	SIGWINCH },
	{ "IO",		SIGIO },
	{ "POLL",	SIGPOLL },
	{ "PWR",	SIGPWR },
	{ "UNUSED",	SIGUNUSED },
#endif
	{ 0,		0		}
};

extern int
kill_main(struct FileInfo * i, int argc, char * * argv)
{
	int	had_error = 0;
	int	sig = SIGTERM;
	if ( argv[1][0] == '-' ) {
		if ( argv[1][1] >= '0' && argv[1][1] <= '9' ) {
			sig = atoi(&argv[1][1]);
			if ( sig < 0 || sig >= NSIG ) {
				usage(kill_usage);
				exit(-1);
			}
		}
		else {
			const struct signal_name *	s = signames;
			for ( ; ; ) {
				if ( strcmp(s->name, &argv[1][1]) == 0 ) {
					sig = s->number;
					break;
				}
				s++;
				if ( s->name == 0 ) {
					usage(kill_usage);
					exit(-1);
				}
			}
		}
		argv++;
		argc--;

	}
	while ( argc > 1 ) {
		int	pid;
		if ( argv[1][0] < '0' || argv[1][0] > '9' ) {
			usage(kill_usage);
			exit(-1);
		}
		pid = atoi(argv[1]);
		if ( kill(pid, sig) != 0 ) {
			had_error = 1;
			perror(argv[1]);
		}
		argv++;
		argc--;
	}
	if ( had_error )
		return -1;
	else
		return 0;
}
