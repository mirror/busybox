/* vi: set sw=4 ts=4: */
/*
 * Mini kill implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
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


#include "internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *kill_usage =
	"kill [-signal] process-id [process-id ...]\n\n"
	"Send a signal (default is SIGTERM) to the specified process(es).\n\n"
	"Options:\n" "\t-l\tList all signal names and numbers.\n\n";

#ifdef BB_KILLALL
static const char *killall_usage =
	"killall [-signal] process-name [process-name ...]\n\n"
	"Send a signal (default is SIGTERM) to the specified process(es).\n\n"
	"Options:\n" "\t-l\tList all signal names and numbers.\n\n";
#endif

#define KILL	0
#define KILLALL	1

struct signal_name {
	const char *name;
	int number;
};

const struct signal_name signames[] = {
	{"HUP", SIGHUP},
	{"INT", SIGINT},
	{"QUIT", SIGQUIT},
	{"ILL", SIGILL},
	{"TRAP", SIGTRAP},
	{"ABRT", SIGABRT},
#ifndef __alpha__
	{"IOT", SIGIOT},
#endif
#if defined(__sparc__) || defined(__alpha__)
	{"EMT", SIGEMT},
#else
	{"BUS", SIGBUS},
#endif
	{"FPE", SIGFPE},
	{"KILL", SIGKILL},
#if defined(__sparc__) || defined(__alpha__)
	{"BUS", SIGBUS},
#else
	{"USR1", SIGUSR1},
#endif
	{"SEGV", SIGSEGV},
#if defined(__sparc__) || defined(__alpha__)
	{"SYS", SIGSYS},
#else
	{"USR2", SIGUSR2},
#endif
	{"PIPE", SIGPIPE},
	{"ALRM", SIGALRM},
	{"TERM", SIGTERM},
#if defined(__sparc__) || defined(__alpha__)
	{"URG", SIGURG},
	{"STOP", SIGSTOP},
	{"TSTP", SIGTSTP},
	{"CONT", SIGCONT},
	{"CHLD", SIGCHLD},
	{"TTIN", SIGTTIN},
	{"TTOU", SIGTTOU},
	{"IO", SIGIO},
# ifndef __alpha__
	{"POLL", SIGIO},
# endif
	{"XCPU", SIGXCPU},
	{"XFSZ", SIGXFSZ},
	{"VTALRM", SIGVTALRM},
	{"PROF", SIGPROF},
	{"WINCH", SIGWINCH},
# ifdef __alpha__
	{"INFO", SIGINFO},
# else
	{"LOST", SIGLOST},
# endif
	{"USR1", SIGUSR1},
	{"USR2", SIGUSR2},
#else
	{"STKFLT", SIGSTKFLT},
	{"CHLD", SIGCHLD},
	{"CONT", SIGCONT},
	{"STOP", SIGSTOP},
	{"TSTP", SIGTSTP},
	{"TTIN", SIGTTIN},
	{"TTOU", SIGTTOU},
	{"URG", SIGURG},
	{"XCPU", SIGXCPU},
	{"XFSZ", SIGXFSZ},
	{"VTALRM", SIGVTALRM},
	{"PROF", SIGPROF},
	{"WINCH", SIGWINCH},
	{"IO", SIGIO},
	{"POLL", SIGPOLL},
	{"PWR", SIGPWR},
	{"UNUSED", SIGUNUSED},
#endif
	{0, 0}
};

extern int kill_main(int argc, char **argv)
{
	int whichApp, sig = SIGTERM;
	const char *appUsage;

#ifdef BB_KILLALL
	/* Figure out what we are trying to do here */
	whichApp = (strcmp(*argv, "killall") == 0)? 
		KILLALL : KILL; 
	appUsage = (whichApp == KILLALL)?  killall_usage : kill_usage;
#else
	whichApp = KILL;
	appUsage = kill_usage;
#endif

	argc--;
	argv++;
	/* Parse any options */
	if (argc < 1)
		usage(appUsage);

	while (argc > 0 && **argv == '-') {
		while (*++(*argv)) {
			switch (**argv) {
			case 'l':
				{
					int col = 0;
					const struct signal_name *s = signames;

					while (s->name != 0) {
						col +=
							fprintf(stderr, "%2d) %-8s", s->number,
									(s++)->name);
						if (col > 60) {
							fprintf(stderr, "\n");
							col = 0;
						}
					}
					fprintf(stderr, "\n\n");
					exit(TRUE);
				}
				break;
			case '-':
				usage(appUsage);
			default:
				{
					if (isdigit(**argv)) {
						sig = atoi(*argv);
						if (sig < 0 || sig >= NSIG)
							goto end;
						else {
							argc--;
							argv++;
							goto do_it_now;
						}
					} else {
						const struct signal_name *s = signames;

						while (s->name != 0) {
							if (strcasecmp(s->name, *argv) == 0) {
								sig = s->number;
								argc--;
								argv++;
								goto do_it_now;
							}
							s++;
						}
						if (s->name == 0)
							goto end;
					}
				}
			}
			argc--;
			argv++;
		}
	}

  do_it_now:

	if (whichApp == KILL) {
		/* Looks like they want to do a kill. Do that */
		while (--argc >= 0) {
			int pid;

			if (!isdigit(**argv))
				fatalError( "Bad PID: %s\n", strerror(errno));
			pid = strtol(*argv, NULL, 0);
			if (kill(pid, sig) != 0) 
				fatalError( "Could not kill pid '%d': %s\n", pid, strerror(errno));
			argv++;
		}
	} 
#ifdef BB_KILLALL
	else {
		/* Looks like they want to do a killall.  Do that */
		while (--argc >= 0) {
			int pid;

			while((pid = findPidByName( *argv))) {
				if (kill(pid, sig) != 0) 
					fatalError( "Could not kill pid '%d': %s\n", pid, strerror(errno));
			}
			argv++;
		}
	}
#endif

	exit(TRUE);


  end:
	fatalError( "bad signal name: %s\n", *argv);
}
