/* vi: set sw=4 ts=4: */
/*
 * Mini mkdir implementation for busybox
 *
 * Copyright (C) 1999,2000,2001 by Lineo, inc.
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
#define bb_need_name_too_long
#define BB_DECLARE_EXTERN
#include "messages.c"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

static int parentFlag = FALSE;
static mode_t mode = 0777;


extern int mkdir_main(int argc, char **argv)
{
	int i = FALSE;

	argc--;
	argv++;

	/* Parse any options */
	while (argc > 0 && **argv == '-') {
		while (i == FALSE && *++(*argv)) {
			switch (**argv) {
			case 'm':
				if (--argc == 0)
					usage(mkdir_usage);
				/* Find the specified modes */
				mode = 0;
				if (parse_mode(*(++argv), &mode) == FALSE) {
					error_msg("Unknown mode: %s\n", *argv);
					return EXIT_FAILURE;
				}
				/* Set the umask for this process so it doesn't 
				 * screw up whatever the user just entered. */
				umask(0);
				i = TRUE;
				break;
			case 'p':
				parentFlag = TRUE;
				break;
			default:
				usage(mkdir_usage);
			}
		}
		argc--;
		argv++;
	}

	if (argc < 1) {
		usage(mkdir_usage);
	}

	while (argc > 0) {
		int status;
		struct stat statBuf;
		char buf[BUFSIZ + 1];

		if (strlen(*argv) > BUFSIZ - 1) {
			error_msg(name_too_long);
			return EXIT_FAILURE;
		}
		strcpy(buf, *argv);
		status = stat(buf, &statBuf);
		if (parentFlag == FALSE && status != -1 && errno != ENOENT) {
			error_msg("%s: File exists\n", buf);
			return EXIT_FAILURE;
		}
		if (parentFlag == TRUE) {
			strcat(buf, "/");
			create_path(buf, mode);
		} else {
			if (mkdir(buf, mode) != 0 && parentFlag == FALSE) {
				perror(buf);
				return EXIT_FAILURE;
			}
		}
		argc--;
		argv++;
	}
	return EXIT_SUCCESS;
}
