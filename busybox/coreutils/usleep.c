/* vi: set sw=4 ts=4: */
/*
 * usleep implementation for busybox
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

/* BB_AUDIT SUSv3 N/A -- Apparently a busybox extension. */

#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include "busybox.h"

extern int usleep_main(int argc, char **argv)
{
	if (argc != 2) {
		bb_show_usage();
	}

	if (usleep(bb_xgetularg10_bnd(argv[1], 0, UINT_MAX))) {
		bb_perror_nomsg_and_die();
	}

	return EXIT_SUCCESS;
}
