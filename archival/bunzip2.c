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

int bunzip2_main(int argc, char **argv)
{
	const int bunzip_to_stdout = 1;
	const int bunzip_force = 2;
	int flags = 0;
	int opt = 0;
	int status;

	int src_fd;
	int dst_fd;
	char *save_name = NULL;
	char *delete_name = NULL;

	/* if called as bzcat */
	if (strcmp(bb_applet_name, "bzcat") == 0)
		flags |= bunzip_to_stdout;

	while ((opt = getopt(argc, argv, "cfh")) != -1) {
		switch (opt) {
		case 'c':
			flags |= bunzip_to_stdout;
			break;
		case 'f':
			flags |= bunzip_force;
			break;
		case 'h':
		default:
			bb_show_usage(); /* exit's inside usage */
		}
	}

	/* Set input filename and number */
	if (argv[optind] == NULL || strcmp(argv[optind], "-") == 0) {
		flags |= bunzip_to_stdout;
		src_fd = fileno(stdin);
	} else {
		/* Open input file */
		src_fd = bb_xopen(argv[optind], O_RDONLY);

		save_name = bb_xstrdup(argv[optind]);
		if (strcmp(save_name + strlen(save_name) - 4, ".bz2") != 0)
			bb_error_msg_and_die("Invalid extension");
		save_name[strlen(save_name) - 4] = '\0';
	}

	/* Check that the input is sane.  */
	if (isatty(src_fd) && (flags & bunzip_force) == 0) {
		bb_error_msg_and_die("compressed data not read from terminal.  Use -f to force it.");
	}

	if (flags & bunzip_to_stdout) {
		dst_fd = fileno(stdout);
	} else {
		dst_fd = bb_xopen(save_name, O_WRONLY | O_CREAT);
	}

	status = uncompressStream(src_fd, dst_fd);
	if(!(flags & bunzip_to_stdout)) {
		if (status) {
			delete_name = save_name;
		} else {
			delete_name = argv[optind];
		}
	}

	if ((delete_name) && (unlink(delete_name) < 0)) {
		bb_error_msg_and_die("Couldn't remove %s", delete_name);
	}

	return status;
}
