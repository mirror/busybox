/* vi: set sw=4 ts=4: */
/*
 * Mini rm implementation for busybox
 *
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
 *
 * INTERACTIVE feature Copyright (C) 2001 by Alcove
 *   written by Christophe Boyanique <Christophe.Boyanique@fr.alcove.com>
 *   for Ipanema Technologies
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
#include <time.h>
#include <utime.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include "busybox.h"

static int recursiveFlag = FALSE;
static int forceFlag = FALSE;
#ifdef BB_FEATURE_RM_INTERACTIVE
	static int interactiveFlag = FALSE;
#endif
static const char *srcName;


static int fileAction(const char *fileName, struct stat *statbuf, void* junk)
{
#ifdef BB_FEATURE_RM_INTERACTIVE
	if (interactiveFlag == TRUE) {
		printf("rm: remove `%s'? ", fileName);
		if (ask_confirmation() == 0)
			return (TRUE);
	}
#endif
	if (unlink(fileName) < 0) {
		perror_msg("%s", fileName);
		return (FALSE);
	}
	return (TRUE);
}

static int dirAction(const char *fileName, struct stat *statbuf, void* junk)
{
	if (recursiveFlag == FALSE) {
		errno = EISDIR;
		perror_msg("%s", fileName);
		return (FALSE);
	} 
#ifdef BB_FEATURE_RM_INTERACTIVE
	if (interactiveFlag == TRUE) {
		printf("rm: remove directory `%s'? ", fileName);
		if (ask_confirmation() == 0)
			return (TRUE);
	}
#endif
	if (rmdir(fileName) < 0) {
		perror_msg("%s", fileName);
		return (FALSE);
	}
	return (TRUE);
}

extern int rm_main(int argc, char **argv)
{
	int opt;
	int status = EXIT_SUCCESS;
	struct stat statbuf;
	
	
	/* do normal option parsing */
	while ((opt = getopt(argc, argv, "Rrf-"
#ifdef BB_FEATURE_RM_INTERACTIVE
"i"
#endif
)) > 0) {
		switch (opt) {
			case 'R':
			case 'r':
				recursiveFlag = TRUE;
				break;
			case 'f':
				forceFlag = TRUE;
#ifdef BB_FEATURE_RM_INTERACTIVE

				interactiveFlag = FALSE;
#endif
				break;
#ifdef BB_FEATURE_RM_INTERACTIVE
			case 'i':
				interactiveFlag = TRUE;
				forceFlag = FALSE;
				break;
#endif
			default:
				show_usage();
		}
	}
	
	if (argc == optind && forceFlag == FALSE) {
		show_usage();
	}

	while (optind < argc) {
		srcName = argv[optind];
		if (forceFlag == TRUE && lstat(srcName, &statbuf) != 0
			&& errno == ENOENT) {
			/* do not reports errors for non-existent files if -f, just skip them */
		} else {
			if (recursive_action(srcName, recursiveFlag, FALSE,
								TRUE, fileAction, dirAction, NULL) == FALSE) {
				status = EXIT_FAILURE;
			}
		}
		optind++;
	}
	return status;
}
