/* vi: set sw=4 ts=4: */
/*
 * Mini kill/killall implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "busybox.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>

int kill_main(int argc, char **argv)
{
	int killall, signo = SIGTERM, errors = 0, quiet=0;

	killall = (ENABLE_KILLALL && bb_applet_name[4]=='a') ? 1 : 0;

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

			for(signo = 0;;) {
				char *name = get_signame(++signo);
				if (isdigit(*name)) break;

				if (col > 60) {
					printf("\n");
					col = 0;
				}
				col += printf("%2d) %-16s", signo, name);
			}
			printf("\n");
		} else {
			for(argv++; *argv; argv++) {
				char *name;

				if (isdigit(**argv)) name = get_signame(atoi(*argv));
				else {
					int temp = get_signum(*argv);
					if (temp<0)
						bb_error_msg_and_die("unknown signal %s", *argv);
					name = get_signame(temp);
				}
				puts(name);
			}
		}
		/* If they specified -l, were all done */
		return EXIT_SUCCESS;
	}

	/* The -q quiet option */
	if(killall && argv[1][1]=='q' && argv[1][2]=='\0'){
		quiet++;
		argv++;
		argc--;
		if(argc<2 || argv[1][0] != '-'){
			goto do_it_now;
		}
	}

	if(0>(signo = get_signum(argv[1]+1)))
		bb_error_msg_and_die( "bad signal name '%s'", argv[1]+1);
	argv+=2;
	argc-=2;

do_it_now:

	/* Pid or name required */
	if (argc <= 0)
		bb_show_usage();

	if (!killall) {
		/* Looks like they want to do a kill. Do that */
		while (--argc >= 0) {
			int pid;

			if (!isdigit(**argv) && **argv != '-')
				bb_error_msg_and_die( "Bad PID '%s'", *argv);
			pid = strtol(*argv, NULL, 0);
			if (kill(pid, signo) != 0) {
				bb_perror_msg( "Could not kill pid '%d'", pid);
				errors++;
			}
			argv++;
		}

	} else {
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

	return errors;
}
