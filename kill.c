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
#include <unistd.h>
#include <signal.h>
#include <ctype.h>

const char kill_usage[] = "kill [-signal] process-id [process-id ...]\n";

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

extern int kill_main (int argc, char **argv)
{
    int sig = SIGTERM;

    if ( argc < 2 )
	usage (kill_usage);

    if ( **(argv+1) == '-' ) {
	if (isdigit( *(*(++argv)+1) )) {
	    sig = atoi (*argv);
	    if (sig < 0 || sig >= NSIG)
		goto end;
	}
	else {
	    const struct signal_name *s = signames;
	    while (s->name != 0) {
		if (strcasecmp (s->name, *argv+1) == 0) {
		    sig = s->number;
		    break;
		}
		s++;
	    }
	    if (s->name == 0)
		goto end;
	}
    }

    while (--argc > 1) {
	int pid;
	if (! isdigit( **(++argv))) {
	    fprintf(stderr, "bad PID: %s\n", *argv);
	    exit( FALSE);
	}
	pid = atoi (*argv);
	if (kill (pid, sig) != 0) {
	    perror (*argv);
	    exit ( FALSE);
	}
    }

end:
    fprintf(stderr, "bad signal name: %s\n", *argv);
    exit (TRUE);
}


