/* vi: set sw=4 ts=4: */
/*
 * Mini kill/killall implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
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

#define KILL 0
#define KILLALL 1

extern int kill_main(int argc, char **argv)
{
	int whichApp, signo = SIGTERM;
	const char *name;
	int errors = 0;

#ifdef CONFIG_KILLALL
	int quiet=0;
	/* Figure out what we are trying to do here */
	whichApp = (strcmp(bb_applet_name, "killall") == 0)? KILLALL : KILL;
#else
	whichApp = KILL;
#endif

	/* Parse any options */
	if (argc < 2)
		bb_show_usage();

	if(argv[1][0] != '-'){
		argv++;
		argc--;
		goto do_it_now;
	}

	/* The -l option, which prints out signal names. */
	if(argv[1][1]=='l' && argv[1][2]=='\0'){
		if(argc==2) {
			/* Print the whole signal list */
			int col = 0;
			for(signo=1; signo < NSIG; signo++) {
				name = u_signal_names(0, &signo, 1);
				if(name==NULL)  /* unnamed */
					continue;
				col += printf("%2d) %-16s", signo, name);
				if (col > 60) {
					printf("\n");
					col = 0;
				}
			}
			printf("\n");

		} else {
			for(argv++; *argv; argv++) {
				name = u_signal_names(*argv, &signo, -1);
				if(name!=NULL)
					printf("%s\n", name);
			}
		}
		/* If they specified -l, were all done */
		return EXIT_SUCCESS;
	}

#ifdef CONFIG_KILLALL	
	/* The -q quiet option */
	if(argv[1][1]=='q' && argv[1][2]=='\0'){
		quiet++;
		argv++;
		argc--;
		if(argc<2 || argv[1][0] != '-'){
			goto do_it_now;
		}
	}
#endif

	if(!u_signal_names(argv[1]+1, &signo, 0))
		bb_error_msg_and_die( "bad signal name '%s'", argv[1]+1);
	argv+=2;
	argc-=2;

do_it_now:

	if (whichApp == KILL) {
		/* Looks like they want to do a kill. Do that */
		while (--argc >= 0) {
			int pid;

			if (!isdigit(**argv))
				bb_error_msg_and_die( "Bad PID '%s'", *argv);
			pid = strtol(*argv, NULL, 0);
			if (kill(pid, signo) != 0) {
				bb_perror_msg( "Could not kill pid '%d'", pid);
				errors++;
			}
			argv++;
		}

	}
#ifdef CONFIG_KILLALL
	else {
		pid_t myPid=getpid();
		/* Looks like they want to do a killall.  Do that */
		while (--argc >= 0) {
			long* pidList;

			pidList = find_pid_by_name(*argv);
			if (!pidList || *pidList<=0) {
				errors++;
				if (quiet==0)
					bb_error_msg( "%s: no process killed", *argv);
			} else {
				long *pl;

				for(pl = pidList; *pl !=0 ; pl++) {
					if (*pl==myPid)
						continue;
					if (kill(*pl, signo) != 0) {
						errors++;
						if (quiet==0)
							bb_perror_msg( "Could not kill pid '%ld'", *pl);
					}
				}
			}
			free(pidList);
			argv++;
		}
	}
#endif
	return errors;
}
