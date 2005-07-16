/* vi: set sw=4 ts=4: */
/*
 * Mini yes implementation for busybox
 *
 * Copyright (C) 2000  Edward Betts <edward@debian.org>.
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
#include <stdlib.h>
#include "busybox.h"

extern int yes_main(int argc, char **argv)
{
	int i;

	if (argc >= 2 && *argv[1] == '-')
		show_usage();

	if (argc == 1) {
		while (1)
			if (puts("y") == EOF) {
				perror("yes");
				return EXIT_FAILURE;
			}
	}

	while (1)
		for (i = 1; i < argc; i++)
			if (fputs(argv[i], stdout) == EOF
				|| putchar(i == argc - 1 ? '\n' : ' ') == EOF) {
				perror("yes");
				return EXIT_FAILURE;
			}

	return EXIT_SUCCESS;
}
