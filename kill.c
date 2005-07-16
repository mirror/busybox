/* vi: set sw=4 ts=4: */
/*
 * Mini kill/killall implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 * Copyright (C) 1999-2002 by Erik Andersen <andersee@debian.org>
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


extern int kill_main(int argc, char **argv)
{
	int whichApp, sig = SIGTERM, quiet, errors;
	const char *name;

#ifdef BB_KILLALL
	/* Figure out what we are trying to do here */
	whichApp = (strcmp(applet_name, "killall") == 0)? KILLALL : KILL; 
#else
	whichApp = KILL;
#endif

	errors=0;
	quiet=0;
	argc--;
	argv++;
	/* Parse any options */
	if (argc < 1)
		show_usage();

	while (argc > 0 && **argv == '-') {
		while (*++(*argv)) {
			switch (**argv) {
#ifdef BB_KILLALL
				case 'q':
					quiet++;
					break;
#endif
				case 'l':
					if(argc>1) {
						for(argv++; *argv; argv++) {
							name = u_signal_names(*argv, &sig, -1);
							if(name!=NULL)
								printf("%s\n", name);
						}
					} else {
						int col = 0;
						for(sig=1; sig < NSIG; sig++) {
							name = u_signal_names(0, &sig, 1);
							if(name==NULL)  /* unnamed */
								continue;
							col += printf("%2d) %-16s", sig, name);
							if (col > 60) {
								printf("\n");
								col = 0;
							}
						}
						printf("\n");
					}
					return EXIT_SUCCESS;
				case '-':
					show_usage();
				default:
					name = u_signal_names(*argv, &sig, 0);
					if(name==NULL)
						error_msg_and_die( "bad signal name: %s", *argv);
					argc--;
					argv++;
					goto do_it_now;
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
			if (kill(pid, sig) != 0) {
				perror_msg( "Could not kill pid '%d'", pid);
				errors++;
			}
			argv++;
		}
	} 
#ifdef BB_KILLALL
	else {
		pid_t myPid=getpid();
		/* Looks like they want to do a killall.  Do that */
		while (--argc >= 0) {
			long* pidList;

			pidList = find_pid_by_name(*argv);
			if (!pidList || *pidList<=0) {
				errors++;
				if (!quiet)
					error_msg( "%s: no process killed", *argv);
			} else {
				for(; *pidList!=0; pidList++) {
					if (*pidList==myPid)
						continue;
					if (kill(*pidList, sig) != 0) {
						errors++;
						if (!quiet)
							perror_msg( "Could not kill pid '%d'", *pidList);
					}
				}
			}

			/* Note that we don't bother to free the memory
			 * allocated in find_pid_by_name().  It will be freed
			 * upon exit, so we can save a byte or two */
			argv++;
		}
	}
#endif

	return errors;
}
