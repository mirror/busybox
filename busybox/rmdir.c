/* vi: set sw=4 ts=4: */
/*
 * Mini rmdir implementation for busybox
 *
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc.
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

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include "busybox.h"

extern int rmdir_main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;

	if (argc == 1 || **(argv + 1) == '-')
		show_usage();

	while (--argc > 0) {
		if (rmdir(*(++argv)) == -1) {
			perror_msg("%s", *argv);
			status = EXIT_FAILURE;
		}
	}
	return status;
}
