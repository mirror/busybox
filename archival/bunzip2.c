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

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
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

	FILE *src_stream;
	FILE *dst_stream;
	char *save_name = NULL;
	char *delete_name = NULL;

	/* if called as bzcat */
	if (strcmp(applet_name, "bzcat") == 0)
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
			show_usage(); /* exit's inside usage */
		}
	}

	/* Set input filename and number */
	if (argv[optind] == NULL || strcmp(argv[optind], "-") == 0) {
		flags |= bunzip_to_stdout;
		src_stream = stdin;
	} else {
		/* Open input file */
		src_stream = xfopen(argv[optind], "r");

		save_name = xstrdup(argv[optind]);
		if (strcmp(save_name + strlen(save_name) - 4, ".bz2") != 0)
			error_msg_and_die("Invalid extension");
		save_name[strlen(save_name) - 4] = '\0';
	}

	/* Check that the input is sane.  */
	if (isatty(fileno(src_stream)) && (flags & bunzip_force) == 0)
		error_msg_and_die("compressed data not read from terminal.  Use -f to force it.");

	if (flags & bunzip_to_stdout) {
		dst_stream = stdout;
	} else {
		dst_stream = xfopen(save_name, "w");
	}

	if (uncompressStream(src_stream, dst_stream)) {
		if (!(flags & bunzip_to_stdout))
			delete_name = argv[optind];
		status = EXIT_SUCCESS;
	} else {
		if (!(flags & bunzip_to_stdout))
			delete_name = save_name;
		status = EXIT_FAILURE;
	}

	if (delete_name) {
		if (unlink(delete_name) < 0) {
			error_msg_and_die("Couldn't remove %s", delete_name);
		}
	}

	return status;
}
