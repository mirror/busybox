/* vi: set sw=4 ts=4: */
/*
 * Mini basename implementation for busybox
 *
 * Copyright (C) 1999-2003 by Erik Andersen <andersen@codepoet.org>
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
/* http://www.opengroup.org/onlinepubs/007904975/utilities/basename.html */


/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Changes:
 * 1) Now checks for too many args.  Need at least one and at most two.
 * 2) Don't check for options, as per SUSv3.
 * 3) Save some space by using strcmp().  Calling strncmp() here was silly.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "busybox.h"

extern int basename_main(int argc, char **argv)
{
	size_t m, n;
	char *s;

	if (((unsigned int)(argc-2)) >= 2) {
		bb_show_usage();
	}

	s = bb_get_last_path_component(*++argv);

	if (*++argv) {
		n = strlen(*argv);
		m = strlen(s);
		if ((m > n) && ((strcmp)(s+m-n, *argv) == 0)) {
			s[m-n] = '\0';
		}
	}

	puts(s);

	bb_fflush_stdout_and_exit(EXIT_SUCCESS);
}
