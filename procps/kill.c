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


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include "busybox.h"

static const int KILL = 0;
static const int KILLALL = 1;

struct signal_name {
	const char *name;
	int number;
};

static const struct signal_name signames[] = {
	/* POSIX signals */
	{ "HUP",	SIGHUP },	/* 1 */
	{ "INT",	SIGINT }, 	/* 2 */
	{ "QUIT",	SIGQUIT }, 	/* 3 */
	{ "ILL",	SIGILL }, 	/* 4 */
	{ "ABRT",	SIGABRT }, 	/* 6 */
	{ "FPE",	SIGFPE }, 	/* 8 */
	{ "KILL",	SIGKILL }, 	/* 9 */
	{ "SEGV",	SIGSEGV }, 	/* 11 */
	{ "PIPE",	SIGPIPE }, 	/* 13 */
	{ "ALRM",	SIGALRM }, 	/* 14 */
	{ "TERM",	SIGTERM }, 	/* 15 */
	{ "USR1",	SIGUSR1 }, 	/* 10 (arm,i386,m68k,ppc), 30 (alpha,sparc*), 16 (mips) */
	{ "USR2",	SIGUSR2 }, 	/* 12 (arm,i386,m68k,ppc), 31 (alpha,sparc*), 17 (mips) */
	{ "CHLD",	SIGCHLD }, 	/* 17 (arm,i386,m68k,ppc), 20 (alpha,sparc*), 18 (mips) */
	{ "CONT",	SIGCONT }, 	/* 18 (arm,i386,m68k,ppc), 19 (alpha,sparc*), 25 (mips) */
	{ "STOP",	SIGSTOP },	/* 19 (arm,i386,m68k,ppc), 17 (alpha,sparc*), 23 (mips) */
	{ "TSTP",	SIGTSTP },	/* 20 (arm,i386,m68k,ppc), 18 (alpha,sparc*), 24 (mips) */
	{ "TTIN",	SIGTTIN },	/* 21 (arm,i386,m68k,ppc,alpha,sparc*), 26 (mips) */
	{ "TTOU",	SIGTTOU },	/* 22 (arm,i386,m68k,ppc,alpha,sparc*), 27 (mips) */
	/* Miscellaneous other signals */
#ifdef SIGTRAP
	{ "TRAP",	SIGTRAP },	/* 5 */
#endif
#ifdef SIGIOT
	{ "IOT",	SIGIOT }, 	/* 6, same as SIGABRT */
#endif
#ifdef SIGEMT
	{ "EMT",	SIGEMT }, 	/* 7 (mips,alpha,sparc*) */
#endif
#ifdef SIGBUS
	{ "BUS",	SIGBUS },	/* 7 (arm,i386,m68k,ppc), 10 (mips,alpha,sparc*) */
#endif
#ifdef SIGSYS
	{ "SYS",	SIGSYS }, 	/* 12 (mips,alpha,sparc*) */
#endif
#ifdef SIGSTKFLT
	{ "STKFLT",	SIGSTKFLT },	/* 16 (arm,i386,m68k,ppc) */
#endif
#ifdef SIGURG
	{ "URG",	SIGURG },	/* 23 (arm,i386,m68k,ppc), 16 (alpha,sparc*), 21 (mips) */
#endif
#ifdef SIGIO
	{ "IO",		SIGIO },	/* 29 (arm,i386,m68k,ppc), 23 (alpha,sparc*), 22 (mips) */
#endif
#ifdef SIGPOLL
	{ "POLL",	SIGPOLL },	/* same as SIGIO */
#endif
#ifdef SIGCLD
	{ "CLD",	SIGCLD },	/* same as SIGCHLD (mips) */
#endif
#ifdef SIGXCPU
	{ "XCPU",	SIGXCPU },	/* 24 (arm,i386,m68k,ppc,alpha,sparc*), 30 (mips) */
#endif
#ifdef SIGXFSZ
	{ "XFSZ",	SIGXFSZ },	/* 25 (arm,i386,m68k,ppc,alpha,sparc*), 31 (mips) */
#endif
#ifdef SIGVTALRM
	{ "VTALRM",	SIGVTALRM },	/* 26 (arm,i386,m68k,ppc,alpha,sparc*), 28 (mips) */
#endif
#ifdef SIGPROF
	{ "PROF",	SIGPROF },	/* 27 (arm,i386,m68k,ppc,alpha,sparc*), 29 (mips) */
#endif
#ifdef SIGPWR
	{ "PWR",	SIGPWR },	/* 30 (arm,i386,m68k,ppc), 29 (alpha,sparc*), 19 (mips) */
#endif
#ifdef SIGINFO
	{ "INFO",	SIGINFO },	/* 29 (alpha) */
#endif
#ifdef SIGLOST
	{ "LOST",	SIGLOST }, 	/* 29 (arm,i386,m68k,ppc,sparc*) */
#endif
#ifdef SIGWINCH
	{ "WINCH",	SIGWINCH },	/* 28 (arm,i386,m68k,ppc,alpha,sparc*), 20 (mips) */
#endif
#ifdef SIGUNUSED
	{ "UNUSED",	SIGUNUSED },	/* 31 (arm,i386,m68k,ppc) */
#endif
	{0, 0}
};

extern int kill_main(int argc, char **argv)
{
	int whichApp, sig = SIGTERM;

#ifdef BB_KILLALL
	/* Figure out what we are trying to do here */
	whichApp = (strcmp(applet_name, "killall") == 0)? KILLALL : KILL; 
#else
	whichApp = KILL;
#endif

	argc--;
	argv++;
	/* Parse any options */
	if (argc < 1)
		show_usage();

	while (argc > 0 && **argv == '-') {
		while (*++(*argv)) {
			switch (**argv) {
			case 'l':
				{
					int col = 0;
					const struct signal_name *s = signames;

					while (s->name != 0) {
						col += fprintf(stderr, "%2d) %-8s", s->number, s->name);
						s++;
						if (col > 60) {
							fprintf(stderr, "\n");
							col = 0;
						}
					}
					fprintf(stderr, "\n\n");
					return EXIT_SUCCESS;
				}
				break;
			case '-':
				show_usage();
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
				perror_msg_and_die( "Bad PID");
			pid = strtol(*argv, NULL, 0);
			if (kill(pid, sig) != 0) 
				perror_msg_and_die( "Could not kill pid '%d'", pid);
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

			pidList = find_pid_by_name( *argv);
			if (!pidList || *pidList<=0) {
				all_found = FALSE;
				error_msg_and_die( "%s: no process killed", *argv);
			}

			for(; pidList && *pidList!=0; pidList++) {
				if (*pidList==myPid)
					continue;
				if (kill(*pidList, sig) != 0) 
					perror_msg_and_die( "Could not kill pid '%d'", *pidList);
			}
			/* Note that we don't bother to free the memory
			 * allocated in find_pid_by_name().  It will be freed
			 * upon exit, so we can save a byte or two */
			argv++;
		}
		if (all_found == FALSE)
			return EXIT_FAILURE;
	}
#endif

	return EXIT_SUCCESS;


  end:
	error_msg_and_die( "bad signal name: %s", *argv);
}
