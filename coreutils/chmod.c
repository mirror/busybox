/* vi: set sw=4 ts=4: */
/*
 * Mini chmod implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "busybox.h"

static int fileAction(const char *fileName, struct stat *statbuf, void* junk)
{
	if (!parse_mode((char *)junk, &(statbuf->st_mode)))
		error_msg_and_die( "unknown mode: %s", (char *)junk);
	if (chmod(fileName, statbuf->st_mode) == 0)
		return (TRUE);
	perror(fileName);
	return (FALSE);
}

int chmod_main(int argc, char **argv)
{
	int opt;
	int recursiveFlag = FALSE;
	int modeind = 0;   /* Index of the mode argument in `argv'. */
	char *smode;
	static const char chmod_modes[] = "Rrwxstugoa,+-=";

	/* do normal option parsing */
	while (1) {
		int thisind = optind ? optind : 1;

		opt = getopt(argc, argv, chmod_modes);
		if (opt == EOF)
				break;
		smode = strchr(chmod_modes, opt);
		if(smode == NULL)
				show_usage();
		if(smode == chmod_modes) {      /* 'R' */
			recursiveFlag = TRUE;
		} else {
		      if (modeind != 0 && modeind != thisind)
			show_usage();
		      modeind = thisind;
		}
	}

	if (modeind == 0)
		modeind = optind++;

	opt = optind;
	if (opt >= argc) {
		error_msg_and_die(too_few_args);
	}

	smode = argv[modeind];
	/* Ok, ready to do the deed now */
	for (; opt < argc; opt++) {
		if (! recursive_action (argv[opt], recursiveFlag, FALSE, FALSE, fileAction,
					fileAction, smode)) {
			return EXIT_FAILURE;
		}
	}
	return EXIT_SUCCESS;
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
