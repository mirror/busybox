/* vi: set sw=4 ts=4: */
/*
 * last implementation for busybox
 *
 * Copyright (C) 2003  Erik Andersen <andersen@codepoet.org>
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

#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <utmp.h>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "busybox.h"

#ifndef SHUTDOWN_TIME
#  define SHUTDOWN_TIME 254
#endif

/* Grr... utmp char[] members  do not have to be nul-terminated.
 * Do what we can while still keeping this reasonably small.
 * Note: We are assuming the ut_id[] size is fixed at 4. */

#if (UT_LINESIZE != 32) || (UT_NAMESIZE != 32) || (UT_HOSTSIZE != 256)
#error struct utmp member char[] size(s) have changed!
#endif

extern int last_main(int argc, char **argv)
{
	struct utmp ut;
	int n, file = STDIN_FILENO;

	if (argc > 1) {
		bb_show_usage();
	}
	file = bb_xopen(_PATH_WTMP, O_RDONLY);

	printf("%-10s %-14s %-18s %-12.12s %s\n", "USER", "TTY", "HOST", "LOGIN", "TIME");
	while ((n = safe_read(file, (void*)&ut, sizeof(struct utmp))) != 0) {

		if (n != sizeof(struct utmp)) {
			bb_perror_msg_and_die("short read");
		}

		if (strncmp(ut.ut_line, "~", 1) == 0) {
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

		printf("%-10s %-14s %-18s %-12.12s\n", ut.ut_user, ut.ut_line, ut.ut_host,
				ctime(&(ut.ut_tv.tv_sec)) + 4);
	}

	bb_fflush_stdout_and_exit(EXIT_SUCCESS);
}
