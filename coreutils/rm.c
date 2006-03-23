/* vi: set sw=4 ts=4: */
/*
 * Mini rm implementation for busybox
 *
 * Copyright (C) 2001 Matt Kraai <kraai@alumni.carnegiemellon.edu>
 *
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

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/rm.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Size reduction.
 */

#include <unistd.h>
#include "busybox.h"

extern int rm_main(int argc, char **argv)
{
	int status = 0;
	int flags = 0;
	unsigned long opt;

	bb_opt_complementaly = "f-i:i-f";
	opt = bb_getopt_ulflags(argc, argv, "fiRr");
	if(opt & 1)
				flags |= FILEUTILS_FORCE;
	if(opt & 2)
		flags |= FILEUTILS_INTERACTIVE;
	if(opt & 12)
		flags |= FILEUTILS_RECUR;

	if (*(argv += optind) != NULL) {
		do {
			const char *base = bb_get_last_path_component(*argv);

			if ((base[0] == '.') && (!base[1] || ((base[1] == '.') && !base[2]))) {
				bb_error_msg("cannot remove `.' or `..'");
			} else if (remove_file(*argv, flags) >= 0) {
				continue;
			}
			status = 1;
		} while (*++argv);
	} else if (!(flags & FILEUTILS_FORCE)) {
		bb_show_usage();
	}

	return status;
}
