/*
 * Mini mknod implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
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
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

static const char mknod_usage[] = "mknod [OPTION]... NAME TYPE MAJOR MINOR\n\n"
"Make block or character special files.\n\n"
"Options:\n"
"\tb:\tMake a block (buffered) device.\n"
"\tc or u:\tMake a character (un-buffered) device.\n"
"\tp:\tMake a named pipe. Major and minor are ignored for named pipes.\n";

int
mknod_main(int argc, char** argv)
{
	mode_t	mode = 0;
	dev_t	dev = 0;

	switch(argv[2][0]) {
	case 'c':
	case 'u':
		mode = S_IFCHR;
		break;
	case 'b':
		mode = S_IFBLK;
		break;
	case 'p':
		mode = S_IFIFO;
		break;
	default:
		usage (mknod_usage);
	}

	if ( mode == S_IFCHR || mode == S_IFBLK ) {
		dev = (atoi(argv[3]) << 8) | atoi(argv[4]);
		if ( argc != 5 ) {
		    usage (mknod_usage);
		}
	}

	mode |= 0666;

	if ( mknod(argv[1], mode, dev) != 0 ) {
		perror(argv[1]);
		return( FALSE);
	}
	return( TRUE);
}
