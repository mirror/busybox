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

/* getopt not needed */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "busybox.h"

extern int watch_main(int argc, char **argv)
{
	const char date_argv[2][10] = { "date", "" };
	const char clrscr[] =
		"\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";
	const int header_len = 40;
	char header[header_len + 1];
	int period = 2;
	char **cargv;
	int cargc;
	pid_t pid;
	int old_stdout;
	int i;

	if (argc < 2) {
		show_usage();
	} else {
		cargv = argv + 1;
		cargc = argc - 1;

		/* don't use getopt, because it permutes the arguments */
		if (argc >= 3 && !strcmp(argv[1], "-n")) {
			period = strtol(argv[2], NULL, 10);
			if (period < 1)
				show_usage();
			cargv += 2;
			cargc -= 2;
		}
	}


	/* create header */
	snprintf(header, header_len, "Every %ds: ", period);
	for (i = 0; i < cargc && (strlen(header) + strlen(cargv[i]) < header_len);
		 i++) {
		strcat(header, cargv[i]);
		strcat(header, " ");
	}

	/* fill with blanks */
	for (i = strlen(header); i < header_len; i++)
		header[i] = ' ';

	header[header_len - 1] = '\0';


	/* thanks to lye, who showed me how to redirect stdin/stdout */
	old_stdout = dup(1);

	while (1) {
		printf("%s%s", clrscr, header);
		date_main(1, (char **) date_argv);
		printf("\n");

		pid = vfork();	/* vfork, because of ucLinux */
		if (pid > 0) {
			//parent
			wait(0);
			sleep(period);
		} else if (0 == pid) {
			//child
			close(1);
			dup(old_stdout);
			if (execvp(*cargv, cargv))
				error_msg_and_die("Couldn't run command\n");
		} else {
			error_msg_and_die("Couldn't vfork\n");
		}
	}


	return EXIT_SUCCESS;
}
