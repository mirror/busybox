/*
 *  mesg implementation for busybox
 *
 *  Copyright (c) 2002 Manuel Novoa III  <mjn3@codepoet.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <unistd.h>
#include <stdlib.h>
#include "libbb.h"

#ifdef USE_TTY_GROUP
#define S_IWGRP_OR_S_IWOTH	S_IWGRP
#else
#define S_IWGRP_OR_S_IWOTH	(S_IWGRP | S_IWOTH)
#endif

extern int mesg_main(int argc, char *argv[])
{
	struct stat sb;
	char *tty;
	char c;

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
