/* vi: set sw=4 ts=4: */
/*
 * Mini chown/chmod/chgrp implementation for busybox
 *
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
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
#include "busybox.h"

/* Don't use lchown for libc5 or glibc older then 2.1.x */
#if (__GLIBC__ <= 2) && (__GLIBC_MINOR__ < 1)
#define lchown	chown
#endif


static long gid;

static int fileAction(const char *fileName, struct stat *statbuf, void* junk)
{
	if (lchown(fileName, statbuf->st_uid, (gid == -1) ? statbuf->st_gid : gid) == 0) {
		return (TRUE);
	}
	perror(fileName);
	return (FALSE);
}

int chgrp_main(int argc, char **argv)
{
	int opt;
	int recursiveFlag = FALSE;
	char *p=NULL;

	/* do normal option parsing */
	while ((opt = getopt(argc, argv, "R")) > 0) {
		switch (opt) {
			case 'R':
				recursiveFlag = TRUE;
				break;
			default:
				show_usage();
		}
	}

	if (argc > optind && argc > 2 && argv[optind]) {
		/* Find the selected group */
		gid = strtoul(argv[optind], &p, 10);	/* maybe it's already numeric */
		if (argv[optind] == p)
			gid = my_getgrnam(argv[optind]);
	} else {
		error_msg_and_die(too_few_args);
	}

	/* Ok, ready to do the deed now */
	while (++optind < argc) {
		if (recursive_action (argv[optind], recursiveFlag, FALSE, FALSE, 
					fileAction, fileAction, NULL) == FALSE) {
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
