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

#include "busybox.h"
#define BB_DECLARE_EXTERN
#define bb_need_not_a_directory
#include "messages.c"

#include <stdio.h>
#include <dirent.h>
#include <errno.h>

static int symlinkFlag = FALSE;
static int removeoldFlag = FALSE;
static int followLinks = TRUE;

extern int ln_main(int argc, char **argv)
{
	char *linkName, *dirName=NULL;
	int linkIntoDirFlag;
	int stopIt = FALSE;

	argc--;
	argv++;

	/* Parse any options */
	while (argc > 0 && stopIt == FALSE) {
		if (**argv == '-') {
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
					case '-':
						stopIt = TRUE;
						break;
					default:
						usage(ln_usage);
				}
			argc--;
			argv++;
		}
		else
			break;
	}

	if (argc < 2) {
		usage(ln_usage);
	}

	linkName = argv[argc - 1];

	linkIntoDirFlag = isDirectory(linkName, followLinks, NULL);
	if ((argc >= 3) && linkIntoDirFlag == FALSE) {
		errorMsg(not_a_directory, linkName);
		exit FALSE;
	}

	if (linkIntoDirFlag == TRUE)
		dirName = linkName;

	while (argc-- >= 2) {
		int status;

		if (linkIntoDirFlag == TRUE) {
			char *baseName = get_last_path_component(*argv);
			linkName = (char *)xmalloc(strlen(dirName)+strlen(baseName)+2);
			strcpy(linkName, dirName);
			if(dirName[strlen(dirName)-1] != '/')
				strcat(linkName, "/");
			strcat(linkName,baseName);
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

		if (linkIntoDirFlag == TRUE)
			free(linkName);

		argv++;
	}
	return( TRUE);
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
