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

int kill_main(int argc, char **argv)
{
	char *arg;
	int killall, signo = SIGTERM, errors = 0, quiet = 0;

	killall = (ENABLE_KILLALL && bb_applet_name[4]=='a') ? 1 : 0;

	/* Parse any options */
	argc--;
	arg = *++argv;
	if (argc<1)
		bb_show_usage();

	if (arg[0]!='-') {
		goto do_it_now;
	}

	/* The -l option, which prints out signal names. */
	if (arg[1]=='l' && arg[2]=='\0') {
		const char *name;
		if (argc==1) {
			/* Print the whole signal list */
			int col = 0;
			for (signo = 1; signo<32; signo++) {
				name = get_signame(signo);
				if (isdigit(name[0])) continue;
				if (col > 66) {
					puts("");
					col = 0;
				}
				col += printf("%2d) %-6s", signo, name);
			}
			puts("");
		} else { /* -l <sig list> */
			while ((arg = *++argv)!=NULL) {
				if (isdigit(arg[0])) {
					signo = atoi(arg);
					name = get_signame(signo);
				} else {
					signo = get_signum(arg);
					if (signo<0)
						bb_error_msg_and_die("unknown signal '%s'", arg);
					name = get_signame(signo);
				}
				printf("%2d) %s\n", signo, name);
			}
		}
		/* If they specified -l, we are all done */
		return EXIT_SUCCESS;
	}

	/* The -q quiet option */
	if (killall && arg[1]=='q' && arg[2]=='\0') {
		quiet = 1;
		arg = *++argv;
		argc--;
		if (argc<1) bb_show_usage();
		if (arg[0]!='-') goto do_it_now;
	}

	/* -SIG */
	signo = get_signum(&arg[1]);
	if (signo<0)
		bb_error_msg_and_die("bad signal name '%s'", &arg[1]);
	arg = *++argv;
	argc--;

do_it_now:

	/* Pid or name required */
	if (argc<1)
		bb_show_usage();

	if (!killall) {
		/* Looks like they want to do a kill. Do that */
		while (arg) {
			int pid;

			if (!isdigit(arg[0]) && arg[0]!='-')
				bb_error_msg_and_die("bad pid '%s'", arg);
			pid = strtol(arg, NULL, 0);
			if (kill(pid, signo)!=0) {
				bb_perror_msg("cannot kill pid %d", pid);
				errors++;
			}
			arg = *++argv;
		}

	} else {
		pid_t myPid = getpid();

		/* Looks like they want to do a killall.  Do that */
		while (arg) {
			long* pidList;

			pidList = find_pid_by_name(arg);
			if (!pidList || *pidList<=0) {
				errors++;
				if (quiet==0)
					bb_error_msg("%s: no process killed", arg);
			} else {
				long *pl;

				for (pl = pidList; *pl!=0 ; pl++) {
					if (*pl==myPid)
						continue;
					if (kill(*pl, signo)!=0) {
						errors++;
						if (!quiet)
							bb_perror_msg("cannot kill pid %ld", *pl);
					}
				}
			}
			free(pidList);
			arg = *++argv;
		}
	}

	return errors;
}
