/* vi: set sw=4 ts=4: */
/*
 * Mini run-parts implementation for busybox
 *
 *
 * Copyright (C) 2001 by Emanuele Aina <emanuele.aina@tiscali.it>
 *
 * Based on the Debian run-parts program, version 1.15
 *   Copyright (C) 1996 Jeff Noxon <jeff@router.patch.net>,
 *   Copyright (C) 1996-1999 Guy Maor <maor@debian.org>
 *
 *
 * Licensed under GPL v2, see file LICENSE in this tarball for details.
 */

/* This is my first attempt to write a program in C (well, this is my first
 * attempt to write a program! :-) . */

/* This piece of code is heavily based on the original version of run-parts,
 * taken from debian-utils. I've only removed the long options and a the
 * report mode. As the original run-parts support only long options, I've
 * broken compatibility because the BusyBox policy doesn't allow them.
 * The supported options are:
 * -t			test. Print the name of the files to be executed, without
 *				execute them.
 * -a ARG		argument. Pass ARG as an argument the program executed. It can
 *				be repeated to pass multiple arguments.
 * -u MASK		umask. Set the umask of the program executed to MASK. */

/* TODO
 * done - convert calls to error in perror... and remove error()
 * done - convert malloc/realloc to their x... counterparts
 * done - remove catch_sigchld
 * done - use bb's concat_path_file()
 * done - declare run_parts_main() as extern and any other function as static?
 */

#include "busybox.h"
#include <getopt.h>
#include <stdlib.h>


static const struct option runparts_long_options[] = {
	{ "test",		0,		NULL,		't' },
	{ "umask",		1,		NULL,		'u' },
	{ "arg",		1,		NULL,		'a' },
	{ 0,			0,		0,			0 }
};

extern char **environ;

/* run_parts_main */
/* Process options */
int run_parts_main(int argc, char **argv)
{
	char **args = xmalloc(2 * sizeof(char *));
	unsigned char test_mode = 0;
	unsigned short argcount = 1;
	int opt;

	umask(022);

	while ((opt = getopt_long (argc, argv, "tu:a:",
					runparts_long_options, NULL)) > 0)
	{
		switch (opt) {
			/* Enable test mode */
			case 't':
				test_mode++;
				break;
			/* Set the umask of the programs executed */
			case 'u':
				/* Check and set the umask of the program executed. As stated in the original
				 * run-parts, the octal conversion in libc is not foolproof; it will take the
				 * 8 and 9 digits under some circumstances. We'll just have to live with it.
				 */
				umask(bb_xgetlarg(optarg, 8, 0, 07777));
				break;
			/* Pass an argument to the programs */
			case 'a':
				/* Add an argument to the commands that we will call.
				 * Called once for every argument. */
				args = xrealloc(args, (argcount + 2) * (sizeof(char *)));
				args[argcount++] = optarg;
				break;
			default:
				bb_show_usage();
		}
	}

	/* We require exactly one argument: the directory name */
	if (optind != (argc - 1)) {
		bb_show_usage();
	}

	args[0] = argv[optind];
	args[argcount] = 0;

	return(run_parts(args, test_mode, environ));
}
