/* vi: set sw=4 ts=4: */
/*
 * Mini watch implementation for busybox
 *
 * Copyright (C) 2001 by Michael Habermann <mhabermann@gmx.de>
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

/* BB_AUDIT SUSv3 N/A */
/* BB_AUDIT GNU defects -- only option -n is supported. */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Removed dependency on date_main(), added proper error checking, and
 * reduced size.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <assert.h>
#include <unistd.h>
#include <sys/wait.h>
#include "busybox.h"

extern int watch_main(int argc, char **argv)
{
	const int header_len = 40;
	time_t t;
	pid_t pid;
	unsigned period = 2;
	int old_stdout;
	int len, len2;
	char **watched_argv;
	char header[header_len + 1];

	if (argc < 2) {
		bb_show_usage();
	}

	/* don't use getopt, because it permutes the arguments */
	++argv;
	if ((argc > 3) && !strcmp(*argv, "-n")
	) {
		period = bb_xgetularg10_bnd(argv[1], 1, UINT_MAX);
		argv += 2;
	}
	watched_argv = argv;

	/* create header */

	len = snprintf(header, header_len, "Every %ds:", period);
	/* Don't bother checking for len < 0, as it should never happen.
	 * But, just to be prepared... */
	assert(len >= 0);
	do {
		len2 = strlen(*argv);
		if (len + len2 >= header_len-1) {
			break;
		}
		header[len] = ' ';
		memcpy(header+len+1, *argv, len2);
		len += len2+1;
	} while (*++argv);

	header[len] = 0;

	/* thanks to lye, who showed me how to redirect stdin/stdout */
	old_stdout = dup(1);

	while (1) {
		time(&t);
		/* Use dprintf to avoid fflush()ing stdout. */
		if (dprintf(1, "\033[H\033[J%-*s%s\n", header_len, header, ctime(&t)) < 0) {
			bb_perror_msg_and_die("printf");
		}

		pid = vfork();	/* vfork, because of ucLinux */
		if (pid > 0) {
			//parent
			wait(0);
			sleep(period);
		} else if (0 == pid) {
			//child
			close(1);
			dup(old_stdout);
			if (execvp(*watched_argv, watched_argv)) {
				bb_error_msg_and_die("Couldn't run command\n");
			}
		} else {
			bb_error_msg_and_die("Couldn't vfork\n");
		}
	}
}
