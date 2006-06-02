/* vi: set sw=4 ts=4: */
/*
 * mesg implementation for busybox
 *
 * Copyright (c) 2002 Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include "busybox.h"
#include <unistd.h>
#include <stdlib.h>

#ifdef USE_TTY_GROUP
#define S_IWGRP_OR_S_IWOTH	S_IWGRP
#else
#define S_IWGRP_OR_S_IWOTH	(S_IWGRP | S_IWOTH)
#endif

int mesg_main(int argc, char *argv[])
{
	struct stat sb;
	char *tty;
	char c = 0;

	if ((--argc == 0)
		|| ((argc == 1) && (((c = **++argv) == 'y') || (c == 'n')))) {
		if ((tty = ttyname(STDERR_FILENO)) == NULL) {
			tty = "ttyname";
		} else if (stat(tty, &sb) == 0) {
			if (argc == 0) {
				puts(((sb.st_mode & (S_IWGRP | S_IWOTH)) ==
					  0) ? "is n" : "is y");
				return EXIT_SUCCESS;
			}
			if (chmod
				(tty,
				 (c ==
				  'y') ? sb.st_mode | (S_IWGRP_OR_S_IWOTH) : sb.
				 st_mode & ~(S_IWGRP | S_IWOTH)) == 0) {
				return EXIT_SUCCESS;
			}
		}
		bb_perror_msg_and_die("%s", tty);
	}
	bb_show_usage();
}
