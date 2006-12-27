/* vi: set sw=4 ts=4: */
/*
 * printenv implementation for busybox
 *
 * Copyright (C) 2005 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2005 by Mike Frysinger <vapier@gentoo.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
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

	fflush_stdout_and_exit(0);
}
