/* vi: set sw=4 ts=4: */
/*
 * renice implementation for busybox
 *
 * Copyright (C) 2005  Manuel Novoa III  <mjn3@codepoet.org>
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

/* Notes:
 *   Setting an absolute priority was obsoleted in SUSv2 and removed
 *   in SUSv3.  However, the common linux version of renice does
 *   absolute and not relative.  So we'll continue supporting absolute,
 *   although the stdout logging has been removed since both SUSv2 and
 *   SUSv3 specify that stdout isn't used.
 *
 *   This version is lenient in that it doesn't require any IDs.  The
 *   options -p, -g, and -u are treated as mode switches for the
 *   following IDs (if any).  Multiple switches are allowed.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "busybox.h"

#if (PRIO_PROCESS < CHAR_MIN) || (PRIO_PROCESS > CHAR_MAX)
#error Assumption violated : PRIO_PROCESS value
#endif
#if (PRIO_PGRP < CHAR_MIN) || (PRIO_PGRP > CHAR_MAX)
#error Assumption violated : PRIO_PGRP value
#endif
#if (PRIO_USER < CHAR_MIN) || (PRIO_USER > CHAR_MAX)
#error Assumption violated : PRIO_USER value
#endif

static inline int int_add_no_wrap(int a, int b)
{
	int s = a + b;

	if (b < 0) {
		if (s > a) s = INT_MIN;
	} else {
		if (s < a) s = INT_MAX;
	}

	return s;
}

int renice_main(int argc, char **argv)
{
	static const char Xetpriority_msg[] = "%d : %cetpriority";

	int retval = EXIT_SUCCESS;
	int which = PRIO_PROCESS;	/* Default 'which' value. */
	int use_relative = 0;
	int adjustment, new_priority;
	id_t who;

	++argv;

	/* Check if we are using a relative adjustment. */
	if (argv[0] && (argv[0][0] == '-') && (argv[0][1] == 'n') && !argv[0][2]) {
		use_relative = 1;
		++argv;
	}

	if (!*argv) {				/* No args?  Then show usage. */
		bb_show_usage();
	}

	/* Get the priority adjustment (absolute or relative). */
	adjustment = bb_xgetlarg(*argv, 10, INT_MIN, INT_MAX);

	while (*++argv) {
		/* Check for a mode switch. */
		if ((argv[0][0] == '-') && argv[0][1] && !argv[0][2]) {
			static const char opts[]
				= { 'p', 'g', 'u', 0, PRIO_PROCESS, PRIO_PGRP, PRIO_USER };
			const char *p;
			if ((p = strchr(opts, argv[0][1]))) {
				which = p[4];
				continue;
			}
		}

		/* Process an ID arg. */
		if (which == PRIO_USER) {
			struct passwd *p;
			if (!(p = getpwnam(*argv))) {
				bb_error_msg("unknown user: %s", *argv);
				goto HAD_ERROR;
			}
			who = p->pw_uid;
		} else {
			char *e;
			errno = 0;
			who = strtoul(*argv, &e, 10);
			if (*e || (*argv == e) || errno) {
				bb_error_msg("bad value: %s", *argv);
				goto HAD_ERROR;
			}
		}

		/* Get priority to use, and set it. */
		if (use_relative) {
			int old_priority;

			errno = 0;	 /* Needed for getpriority error detection. */
			old_priority = getpriority(which, who);
			if (errno) {
				bb_perror_msg(Xetpriority_msg, who, 'g');
				goto HAD_ERROR;
			}

			new_priority = int_add_no_wrap(old_priority, adjustment);
		} else {
			new_priority = adjustment;
		}

		if (setpriority(which, who, new_priority) == 0) {
			continue;
		}

		bb_perror_msg(Xetpriority_msg, who, 's');
	HAD_ERROR:
		retval = EXIT_FAILURE;
	}

	/* No need to check for errors outputing to stderr since, if it
	 * was used, the HAD_ERROR label was reached and retval was set. */

	return retval;
}
