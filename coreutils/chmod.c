/* vi: set sw=4 ts=4: */
/*
 * Mini chmod implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Reworked by (C) 2002 Vladimir Oleynik <dzo@simtreas.ru>
 *  to correctly parse '-rwxgoa'
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
/* BB_AUDIT GNU defects - unsupported options -c, -f, -v, and long options. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/chmod.html */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "busybox.h"

static int fileAction(const char *fileName, struct stat *statbuf, void* junk)
{
	if (!bb_parse_mode((char *)junk, &(statbuf->st_mode)))
		bb_error_msg_and_die( "invalid mode: %s", (char *)junk);
	if (chmod(fileName, statbuf->st_mode) == 0)
		return (TRUE);
	bb_perror_msg("%s", fileName);	/* Avoid multibyte problems. */
	return (FALSE);
}

int chmod_main(int argc, char **argv)
{
	int retval = EXIT_SUCCESS;
	int recursiveFlag = FALSE;
	int count;
	char *smode;
	char **p;
	char *p0;
	char opt = '-';

	++argv;
	count = 0;

	for (p = argv  ; *p ; p++) {
		p0 = p[0];
		if (p0[0] == opt) {
			if ((p0[1] == '-') && !p0[2]) {
				opt = 0;	/* Disable further option processing. */
				continue;
			}
			if (p0[1] == 'R') {
				char *s = p0 + 2;
				while (*s == 'R') {
					++s;
				}
				if (*s) {
					bb_show_usage();
				}
				recursiveFlag = TRUE;
				continue;
			}
			if (count) {
				bb_show_usage();
			}
		}
		argv[count] = p0;
		++count;
	}

	argv[count] = NULL;

	if (count < 2) {
		bb_show_usage();
	}

	smode = *argv;
	++argv;

	/* Ok, ready to do the deed now */
	do {
		if (! recursive_action (*argv, recursiveFlag, FALSE, FALSE,
								fileAction,	fileAction, smode)) {
			retval = EXIT_FAILURE;
		}
	} while (*++argv);

	return retval;
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
