/* vi: set sw=4 ts=4: */
/*
 * cat implementation for busybox
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

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/cat.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * This is a new implementation of 'cat' which aims to be SUSv3 compliant.
 *
 * Changes from the previous implementation include:
 * 1) Multiple '-' args are accepted as required by SUSv3.  The previous
 *    implementation would close stdin and segfault on a subsequent '-'.
 * 2) The '-u' options is required by SUSv3.  Note that the specified
 *    behavior for '-u' is done by default, so all we need do is accept
 *    the option.
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "busybox.h"

extern int cat_main(int argc, char **argv)
{
	FILE *f;
	int retval = EXIT_SUCCESS;

	bb_getopt_ulflags(argc, argv, "u");

	argv += optind;
	if (!*argv) {
		*--argv = "-";
	}

	do {
		if ((f = bb_wfopen_input(*argv)) != NULL) {
			int r = bb_copyfd_eof(fileno(f), STDOUT_FILENO);
			bb_fclose_nonstdin(f);
			if (r >= 0) {
				continue;
			}
		}
		retval = EXIT_FAILURE;
	} while (*++argv);

	return retval;
}
