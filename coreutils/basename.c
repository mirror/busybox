/* vi: set sw=4 ts=4: */
/*
 * Mini basename implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
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

#include <stdlib.h>
#include "busybox.h"
#include <string.h>

extern int basename_main(int argc, char **argv)
{
	int m, n;
	char *s;

	if ((argc < 2) || (**(argv + 1) == '-')) {
		show_usage();
	}

	argv++;

	s = get_last_path_component(*argv);

	if (argc>2) {
		argv++;
		n = strlen(*argv);
		m = strlen(s);
		if (m>n && strncmp(s+m-n, *argv, n)==0)
			s[m-n] = '\0';
	}
	puts(s);
	return EXIT_SUCCESS;
}
