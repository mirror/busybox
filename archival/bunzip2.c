/*
 *  Modified for busybox by Glenn McGrath <bug1@optushome.com.au>
 *  Added support output to stdout by Thomas Lundquist <thomasez@zelow.no>
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

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "busybox.h"
#include "unarchive.h"

#define BUNZIP2_OPT_STDOUT	1
#define BUNZIP2_OPT_FORCE	2

int bunzip2_main(int argc, char **argv)
{
	char *compressed_name;
	char *save_name;
	unsigned long opt;
	int status;
	int src_fd;
	int dst_fd;

	opt = bb_getopt_ulflags(argc, argv, "cf");

	/* if called as bzcat force the stdout flag */
	if (bb_applet_name[2] == 'c') {
		opt |= BUNZIP2_OPT_STDOUT;
	}

	/* Set input filename and number */
	compressed_name = argv[optind];
	if ((compressed_name) && (compressed_name[0] != '-') && (compressed_name[1] != '\0')) {
		/* Open input file */
		src_fd = bb_xopen(compressed_name, O_RDONLY);
	} else {
		src_fd = fileno(stdin);
		opt |= BUNZIP2_OPT_STDOUT;
	}

	/* Check that the input is sane.  */
	if (isatty(src_fd) && (opt & BUNZIP2_OPT_FORCE) == 0) {
		bb_error_msg_and_die("compressed data not read from terminal.  Use -f to force it.");
	}

	if (opt & BUNZIP2_OPT_STDOUT) {
		dst_fd = fileno(stdout);
	} else {
		int len = strlen(compressed_name) - 4;
		if (strcmp(compressed_name + len, ".bz2") != 0) {
			bb_error_msg_and_die("Invalid extension");
		}
		save_name = bb_xstrndup(compressed_name, len);
		dst_fd = bb_xopen(save_name, O_WRONLY | O_CREAT);
	}

	status = uncompressStream(src_fd, dst_fd);
	if(!(opt & BUNZIP2_OPT_STDOUT)) {
		char *delete_name;
		if (status) {
			delete_name = save_name;
		} else {
			delete_name = compressed_name;
		}
		if (unlink(delete_name) < 0) {
			bb_error_msg_and_die("Couldn't remove %s", delete_name);
		}
	}

	return status;
}
