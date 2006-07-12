/* vi: set sw=4 ts=4: */
/*
 * nice implementation for busybox
 *
 * Copyright (C) 2005  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <sys/resource.h>
#include "busybox.h"

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

int nice_main(int argc, char **argv)
{
	static const char Xetpriority_msg[] = "cannot %cet priority";

	int old_priority, adjustment;

	errno = 0;			 /* Needed for getpriority error detection. */
	old_priority = getpriority(PRIO_PROCESS, 0);
	if (errno) {
		bb_perror_msg_and_die(Xetpriority_msg, 'g');
	}

	if (!*++argv) {	/* No args, so (GNU) output current nice value. */
		bb_printf("%d\n", old_priority);
		bb_fflush_stdout_and_exit(EXIT_SUCCESS);
	}

	adjustment = 10;			/* Set default adjustment. */

	if ((argv[0][0] == '-') && (argv[0][1] == 'n') && !argv[0][2]) { /* "-n" */
		if (argc < 4) {			/* Missing priority and/or utility! */
			bb_show_usage();
		}
		adjustment = bb_xgetlarg(argv[1], 10, INT_MIN, INT_MAX);
		argv += 2;
	}

	{  /* Set our priority.  Handle integer wrapping for old + adjust. */
		int new_priority = int_add_no_wrap(old_priority, adjustment);

		if (setpriority(PRIO_PROCESS, 0, new_priority) < 0) {
			bb_perror_msg_and_die(Xetpriority_msg, 's');
		}
	}

	execvp(*argv, argv);		/* Now exec the desired program. */

	/* The exec failed... */
	bb_default_error_retval = (errno == ENOENT) ? 127 : 126; /* SUSv3 */
	bb_perror_msg_and_die("%s", *argv);
}
