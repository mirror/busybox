/* vi: set sw=4 ts=4: */
/*
 * yes implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
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

/* BB_AUDIT SUSv3 N/A -- Matches GNU behavior. */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Size reductions and removed redundant applet name prefix from error messages.
 */

#include <stdio.h>
#include <stdlib.h>
#include "busybox.h"

extern int yes_main(int argc, char **argv)
{
	static const char fmt_str[] = " %s";
	const char *fmt;
	char **first_arg;

	*argv = "y";
	if (argc != 1) {
		++argv;
	}

	first_arg = argv;
	do {
		fmt = fmt_str + 1;
		do {
			bb_printf(fmt, *argv);
			fmt = fmt_str;
		} while (*++argv);
		argv = first_arg;
	} while (putchar('\n') != EOF);

	bb_perror_nomsg_and_die();
}
