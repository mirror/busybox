/* vi: set sw=4 ts=4: */
/*
 * Mini whoami implementation for busybox
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

/* BB_AUDIT SUSv3 N/A -- Matches GNU behavior. */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "busybox.h"

extern int whoami_main(int argc, char **argv)
{
	if (argc > 1)
		bb_show_usage();

	puts(my_getpwuid(NULL, geteuid(), -1));
	/* exits on error */
	bb_fflush_stdout_and_exit(EXIT_SUCCESS);
}
