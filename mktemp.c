/* vi: set sw=4 ts=4: */
/*
 * Mini mktemp implementation for busybox
 *
 *
 * Copyright (C) 2000 by Daniel Jacobowitz
 * Written by Daniel Jacobowitz <dan@debian.org>
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

#include "internal.h"
#include <stdio.h>
#include <errno.h>


extern int mktemp_main(int argc, char **argv)
{
	if (argc != 2 && (argc != 3 || strcmp(argv[1], "-q")))
		usage	("mktemp [-q] TEMPLATE\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
				"\nCreates a temporary file with its name based on TEMPLATE.\n"
				"TEMPLATE is any name with six `Xs' (i.e. /tmp/temp.XXXXXX).\n"
#endif
				);
	if(mkstemp(argv[argc-1]) < 0)
			exit(FALSE);
	(void) puts(argv[argc-1]);
	return(TRUE);
}
