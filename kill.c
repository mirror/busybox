/* vi: set sw=4 ts=4: */
/*
 * Mini kill/killall implementation for busybox
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
	"kill [-signal] process-id [process-id ...]\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nSend a signal (default is SIGTERM) to the specified process(es).\n\n"
	"Options:\n" "\t-l\tList all signal names and numbers.\n\n"
#endif
	;

#ifdef BB_KILLALL
static const char *killall_usage =
	"killall [-signal] process-name [process-name ...]\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nSend a signal (default is SIGTERM) to the specified process(es).\n\n"
	"Options:\n" "\t-l\tList all signal names and numbers.\n\n"
#endif
#endif
	;

#define KILL	0
#define KILLALL	1

struct signal_name {
	const char *name;
	int number;
};

const struct signal_name signames[] = {
	/* Everything, order not important */
	{"HUP", SIGHUP},
	{"INT", SIGINT},
	{"QUIT", SIGQUIT},
	{"ILL", SIGILL},
	{"TRAP", SIGTRAP},
	{"ABRT", SIGABRT},
	{"FPE", SIGFPE},
	{"KILL", SIGKILL},
	{"SEGV", SIGSEGV},
	{"PIPE", SIGPIPE},
	{"ALRM", SIGALRM},
	{"TERM", SIGTERM},
	{"BUS", SIGBUS},
	{"USR1", SIGUSR1},
	{"USR2", SIGUSR2},
	{"STOP", SIGSTOP},
	{"CONT", SIGCONT},
	{"TTIN", SIGTTIN},
	{"TTOU", SIGTTOU},
	{"IO", SIGIO},
	{"TSTP", SIGTSTP},
	{"CHLD", SIGCHLD},
	{"XCPU", SIGXCPU},
	{"XFSZ", SIGXFSZ},
	{"PROF", SIGPROF},
	{"WINCH", SIGWINCH},
	{"URG", SIGURG},
	{"VTALRM", SIGVTALRM},
#ifndef __alpha__
	/* everything except alpha */
	{"IOT", SIGIOT},
	{"POLL", SIGPOLL},
#endif
#if defined(__sparc__) || defined(__alpha__) || defined(__mips__)
	/* everthing except intel */
	{"EMT", SIGEMT},
	{"SYS", SIGSYS},
# ifdef __alpha__
		/* alpha only */
		{"LOST", SIGLOST},
#endif
#ifdef __sparc__
		/* space only */
		{"INFO", SIGINFO},
#endif
#ifdef __mips__
		/* mips only */
		{"CLD", SIGCLD},
		{"PWR", SIGPWR},
#endif
#else
	/* intel only */
	{"STKFLT", SIGSTKFLT},
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
		int all_found = TRUE;
		pid_t myPid=getpid();
		/* Looks like they want to do a killall.  Do that */
		while (--argc >= 0) {
			pid_t* pidList;

			pidList = findPidByName( *argv);
			if (!pidList) {
				all_found = FALSE;
				errorMsg( "%s: no process killed\n", *argv);
			}

			for(; pidList && *pidList!=0; pidList++) {
				if (*pidList==myPid)
					continue;
				if (kill(*pidList, sig) != 0) 
					fatalError( "Could not kill pid '%d': %s\n", *pidList, strerror(errno));
			}
			/* Note that we don't bother to free the memory
			 * allocated in findPidByName().  It will be freed
			 * upon exit, so we can save a byte or two */
			argv++;
		}
		exit (all_found);
	}
#endif

	exit(TRUE);


  end:
	fatalError( "bad signal name: %s\n", *argv);
}
