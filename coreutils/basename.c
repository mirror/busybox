/* vi: set sw=4 ts=4: */
/*
 * Mini basename implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
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

int basename_main(int argc, char **argv);
int basename_main(int argc, char **argv)
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

	fflush_stdout_and_exit(EXIT_SUCCESS);
}
