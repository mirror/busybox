/* vi: set sw=4 ts=4: */
/*
 * Mini chvt implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* no options, no getopt */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include "busybox.h"

/* From <linux/vt.h> */
enum {
	VT_ACTIVATE = 0x5606,   /* make vt active */
	VT_WAITACTIVE = 0x5607  /* wait for vt active */
};

int chvt_main(int argc, char **argv)
{
	int fd, num;

	if (argc != 2) {
		bb_show_usage();
	}

	fd = get_console_fd();
	num =  bb_xgetlarg(argv[1], 10, 0, INT_MAX);
	if ((-1 == ioctl(fd, VT_ACTIVATE, num))
	|| (-1 == ioctl(fd, VT_WAITACTIVE, num))) {
		bb_perror_msg_and_die("ioctl");
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
