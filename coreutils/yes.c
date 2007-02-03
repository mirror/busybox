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

#include "busybox.h"

int yes_main(int argc, char **argv);
int yes_main(int argc, char **argv)
{
	static const char fmt_str[] = " %s";
	const char *fmt;
	char **first_arg;

	*argv = (char*)"y";
	if (argc != 1) {
		++argv;
	}

	first_arg = argv;
	do {
		fmt = fmt_str + 1;
		do {
			printf(fmt, *argv);
			fmt = fmt_str;
		} while (*++argv);
		argv = first_arg;
	} while (putchar('\n') != EOF);

	bb_perror_nomsg_and_die();
}
