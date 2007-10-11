/* vi: set sw=4 ts=4: */
/*
 * yes implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 N/A -- Matches GNU behavior. */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Size reductions and removed redundant applet name prefix from error messages.
 */

#include "libbb.h"

/* This is a NOFORK applet. Be very careful! */

int yes_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int yes_main(int argc, char **argv)
{
	char **first_arg;

	argv[0] = (char*)"y";
	if (argc != 1) {
		++argv;
	}

	first_arg = argv;
	do {
		while (1) {
			fputs(*argv, stdout);
			if (!*++argv)
				break;
			putchar(' ');
		}
		argv = first_arg;
	} while (putchar('\n') != EOF);

	bb_perror_nomsg_and_die();
}
