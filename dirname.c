/* vi: set sw=4 ts=4: */
/*
 * Mini dirname implementation for busybox
 *
 * Copyright (C) 2000 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
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
#include "busybox.h"
#include <stdio.h>

extern int dirname_main(int argc, char **argv)
{
	char* s;

	if ((argc < 2) || (**(argv + 1) == '-'))
		usage(dirname_usage);
	argv++;

	s=*argv+strlen(*argv)-1;
	while (s && *s == '/') {
		*s = '\0';
		s=*argv+strlen(*argv)-1;
	}
	s = strrchr(*argv, '/');
	if (s && *s)
		*s = '\0';
	printf("%s\n", (s)? *argv : ".");
	return EXIT_SUCCESS;
}
