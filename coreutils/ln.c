/* vi: set sw=4 ts=4: */
/*
 * Mini ln implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
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
#define BB_DECLARE_EXTERN
#define bb_need_name_too_long
#define bb_need_not_a_directory
#include "messages.c"

#include <stdio.h>
#include <dirent.h>
#include <errno.h>

static const char ln_usage[] =
	"ln [OPTION] TARGET... LINK_NAME|DIRECTORY\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
	"\nCreate a link named LINK_NAME or DIRECTORY to the specified TARGET\n\n"
	"Options:\n"
	"\t-s\tmake symbolic links instead of hard links\n"

	"\t-f\tremove existing destination files\n"
	"\t-n\tno dereference symlinks - treat like normal file\n"
#endif
	;

static int symlinkFlag = FALSE;
static int removeoldFlag = FALSE;
static int followLinks = TRUE;

extern int ln_main(int argc, char **argv)
{
	char *linkName;
	int linkIntoDirFlag;

	if (argc < 3) {
		usage(ln_usage);
	}
	argc--;
	argv++;

	/* Parse any options */
	while (--argc >= 0 && *argv && **argv) {
		while (**argv == '-') {
			while (*++(*argv))
				switch (**argv) {
					case 's':
						symlinkFlag = TRUE;
						break;
					case 'f':
						removeoldFlag = TRUE;
						break;
					case 'n':
						followLinks = FALSE;
						break;
					default:
						usage(ln_usage);
				}
		}
		argv++;
	}

	if (argc < 1) {
		fatalError("ln: missing file argument\n");
	}

	linkName = argv[argc - 1];

	if (strlen(linkName) > BUFSIZ) {
		fprintf(stderr, name_too_long, "ln");
		exit FALSE;
	}

	linkIntoDirFlag = isDirectory(linkName, TRUE, NULL);

	if ((argc > 3) && !linkIntoDirFlag) {
		fprintf(stderr, not_a_directory, "ln", linkName);
		exit FALSE;
	}

	while (argc-- >= 2) {
		char srcName[BUFSIZ + 1];
		int nChars, status;

		if (strlen(*argv) > BUFSIZ) {
			fprintf(stderr, name_too_long, "ln");
			exit FALSE;
		}

		if (followLinks == FALSE) {
			strcpy(srcName, *argv);
		} else {
			/* Warning!  This can silently truncate if > BUFSIZ, but
			   I don't think that there can be one > BUFSIZ anyway. */
			nChars = readlink(*argv, srcName, BUFSIZ);
			srcName[nChars] = '\0';
		}

		if (removeoldFlag == TRUE) {
			status = (unlink(linkName) && errno != ENOENT);
			if (status != 0) {
				perror(linkName);
				exit FALSE;
			}
		}

		if (symlinkFlag == TRUE)
			status = symlink(*argv, linkName);
		else
			status = link(*argv, linkName);
		if (status != 0) {
			perror(linkName);
			exit FALSE;
		}
	}
	exit TRUE;
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
