/* vi: set sw=4 ts=4: */
/*
 * Mini readlink implementation for busybox
 *
 *
 * Copyright (C) 2000 by Lineo, inc.
 * Written by Matt Kraai <kraai@alumni.carnegiemellon.edu>
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
#include <errno.h>
#include <unistd.h>

int readlink_main(int argc, char **argv)
{
	char *buf = NULL;
	int bufsize = 128, size = 128;

	if (argc != 2)
		usage(readlink_usage);

	while (bufsize < size + 1) {
		bufsize *= 2;
		buf = xrealloc(buf, bufsize);
		size = readlink(argv[1], buf, bufsize);
		if (size == -1)
			fatalError("%s: %s\n", argv[1], strerror(errno));
	}

	buf[size] = '\0';
	puts(buf);

	return EXIT_SUCCESS;
}
