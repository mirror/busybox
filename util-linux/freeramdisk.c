/* vi: set sw=4 ts=4: */
/*
 * freeramdisk implementation for busybox
 *
 * Copyright (C) 2000 and written by Emanuele Caratti <wiz@iol.it>
 * Adjusted a bit by Erik Andersen <andersee@debian.org>
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
#include <string.h>
#include <linux/fs.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include "internal.h"



static const char freeramdisk_usage[] =
	"freeramdisk DEVICE\n\n"
	"Free all memory used by the specified ramdisk.\n";

extern int
freeramdisk_main(int argc, char **argv)
{
	char  rname[256] = "/dev/ram";
	int   f;

	if (argc < 2 || ( argv[1] && *argv[1] == '-')) {
		usage(freeramdisk_usage);
	}

	if (argc >1)
		strcpy(rname, argv[1]);

	if ((f = open(rname, O_RDWR)) == -1) {
		fatalError( "freeramdisk: cannot open %s: %s\n", rname, strerror(errno));
	}
	if (ioctl(f, BLKFLSBUF) < 0) {
		fatalError( "freeramdisk: failed ioctl on %s: %s\n", rname, strerror(errno));
	}
	/* Don't bother closing.  Exit does
	 * that, so we can save a few bytes */
	/* close(f); */
	exit(TRUE);
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/

