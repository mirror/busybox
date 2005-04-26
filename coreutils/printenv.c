/*
 * printenv implementation for busybox
 *
 * Copyright (C) 2005 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2005 by Mike Frysinger <vapier@gentoo.org>
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "busybox.h"

int printenv_main(int argc, char **argv)
{
	extern char **environ;
	int e = 0;

	/* no variables specified, show whole env */
	if (argc == 1)
		while (environ[e])
			puts(environ[e++]);

	/* search for specified variables and print them out if found */
	else {
		int i;
		size_t l;
		char *arg, *env;

		for (i=1; (arg = argv[i]); ++i)
			for (; (env = environ[e]); ++e) {
				l = strlen(arg);
				if (!strncmp(env, arg, l) && env[l] == '=')
					puts(env + l + 1);
			}
	}

	bb_fflush_stdout_and_exit(0);
}
