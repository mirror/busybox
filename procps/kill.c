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
	pid_t pid;
	int signo = SIGTERM, errors = 0, quiet = 0;
	const int killall = (ENABLE_KILLALL && applet_name[4] == 'a'
	               && (!ENABLE_KILLALL5 || applet_name[7] != '5'));
	const int killall5 = (ENABLE_KILLALL5 && applet_name[4] == 'a'
	                  && (!ENABLE_KILLALL || applet_name[7] == '5'));

	/* Parse any options */
	argc--;
	arg = *++argv;

	if (argc < 1 || arg[0] != '-') {
		goto do_it_now;
	}

	/* The -l option, which prints out signal names. */
	if (arg[1] == 'l' && arg[2] == '\0') {
		const char *name;
		if (argc == 1) {
			/* Print the whole signal list */
			int col = 0;
			for (signo = 1; signo < 32; signo++) {
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
			while ((arg = *++argv)) {
				if (isdigit(arg[0])) {
					signo = xatoi_u(arg);
					name = get_signame(signo);
				} else {
					signo = get_signum(arg);
					if (signo < 0)
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
	if (killall && arg[1] == 'q' && arg[2] == '\0') {
		quiet = 1;
		arg = *++argv;
		argc--;
		if (argc < 1) bb_show_usage();
		if (arg[0] != '-') goto do_it_now;
	}

	/* -SIG */
	signo = get_signum(&arg[1]);
	if (signo < 0)
		bb_error_msg_and_die("bad signal name '%s'", &arg[1]);
	arg = *++argv;
	argc--;

do_it_now:

	if (killall5) {
		pid_t sid;
		procps_status_t* p = NULL;

// Cannot happen anyway? We don't TERM ourself, we STOP
//		/* kill(-1, sig) on Linux (at least 2.1.x)
//		 * might send signal to the calling process too */
//		signal(SIGTERM, SIG_IGN);
		/* Now stop all processes */
		kill(-1, SIGSTOP);
		/* Find out our own session id */
		pid = getpid();
		sid = getsid(pid);
		/* Now kill all processes except our session */
		while ((p = procps_scan(p, PSSCAN_PID|PSSCAN_SID))) {
			if (p->sid != sid && p->pid != pid && p->pid != 1)
				kill(p->pid, signo);
		}
		/* And let them continue */
		kill(-1, SIGCONT);
		return 0;
	}

	/* Pid or name required for kill/killall */
	if (argc < 1)
		bb_show_usage();

	if (killall) {
		/* Looks like they want to do a killall.  Do that */
		pid = getpid();
		while (arg) {
			pid_t* pidList;

			pidList = find_pid_by_name(arg);
			if (*pidList == 0) {
				errors++;
				if (!quiet)
					bb_error_msg("%s: no process killed", arg);
			} else {
				pid_t *pl;

				for (pl = pidList; *pl; pl++) {
					if (*pl == pid)
						continue;
					if (kill(*pl, signo) == 0)
						continue;
					errors++;
					if (!quiet)
						bb_perror_msg("cannot kill pid %u", (unsigned)*pl);
				}
			}
			free(pidList);
			arg = *++argv;
		}
		return errors;
	}

	/* Looks like they want to do a kill. Do that */
	while (arg) {
		/* Huh?
		if (!isdigit(arg[0]) && arg[0] != '-')
			bb_error_msg_and_die("bad pid '%s'", arg);
		*/
		pid = xatou(arg);
		/* FIXME: better overflow check? */
		if (kill(pid, signo) != 0) {
			bb_perror_msg("cannot kill pid %u", (unsigned)pid);
			errors++;
		}
		arg = *++argv;
	}
	return errors;
}
