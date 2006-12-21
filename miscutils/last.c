/* vi: set sw=4 ts=4: */
/*
 * last implementation for busybox
 *
 * Copyright (C) 2003-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under the GPL version 2, see the file LICENSE in this tarball.
 */

#include "busybox.h"
#include <utmp.h>

#ifndef SHUTDOWN_TIME
#  define SHUTDOWN_TIME 254
#endif

/* Grr... utmp char[] members  do not have to be nul-terminated.
 * Do what we can while still keeping this reasonably small.
 * Note: We are assuming the ut_id[] size is fixed at 4. */

#if defined UT_LINESIZE \
	&& ((UT_LINESIZE != 32) || (UT_NAMESIZE != 32) || (UT_HOSTSIZE != 256))
#error struct utmp member char[] size(s) have changed!
#elif defined __UT_LINESIZE \
	&& ((__UT_LINESIZE != 32) || (__UT_NAMESIZE != 64) || (__UT_HOSTSIZE != 256))
#error struct utmp member char[] size(s) have changed!
#endif

int last_main(int argc, char **argv)
{
	struct utmp ut;
	int n, file = STDIN_FILENO;
	time_t t_tmp;

	if (argc > 1) {
		bb_show_usage();
	}
	file = xopen(bb_path_wtmp_file, O_RDONLY);

	printf("%-10s %-14s %-18s %-12.12s %s\n", "USER", "TTY", "HOST", "LOGIN", "TIME");
	while ((n = safe_read(file, (void*)&ut, sizeof(struct utmp))) != 0) {

		if (n != sizeof(struct utmp)) {
			bb_perror_msg_and_die("short read");
		}

		if (ut.ut_line[0] == '~') {
			if (strncmp(ut.ut_user, "shutdown", 8) == 0)
				ut.ut_type = SHUTDOWN_TIME;
			else if (strncmp(ut.ut_user, "reboot", 6) == 0)
				ut.ut_type = BOOT_TIME;
			else if (strncmp(ut.ut_user, "runlevel", 7) == 0)
				ut.ut_type = RUN_LVL;
		} else {
			if (!ut.ut_name[0] || strcmp(ut.ut_name, "LOGIN") == 0 ||
					ut.ut_name[0] == 0)
			{
				/* Don't bother.  This means we can't find how long
				 * someone was logged in for.  Oh well. */
				continue;
			}
			if (ut.ut_type != DEAD_PROCESS &&
					ut.ut_name[0] && ut.ut_line[0])
			{
				ut.ut_type = USER_PROCESS;
			}
			if (strcmp(ut.ut_name, "date") == 0) {
				if (ut.ut_line[0] == '|') ut.ut_type = OLD_TIME;
				if (ut.ut_line[0] == '{') ut.ut_type = NEW_TIME;
			}
		}

		if (ut.ut_type!=USER_PROCESS) {
			switch (ut.ut_type) {
				case OLD_TIME:
				case NEW_TIME:
				case RUN_LVL:
				case SHUTDOWN_TIME:
					continue;
				case BOOT_TIME:
					strcpy(ut.ut_line, "system boot");
					break;
			}
		}
		t_tmp = (time_t)ut.ut_tv.tv_sec;
		printf("%-10s %-14s %-18s %-12.12s\n", ut.ut_user, ut.ut_line, ut.ut_host,
				ctime(&t_tmp) + 4);
	}

	fflush_stdout_and_exit(EXIT_SUCCESS);
}
