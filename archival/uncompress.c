/* vi: set sw=4 ts=4: */
/*
 *	Uncompress applet for busybox (c) 2002 Glenn McGrath
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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "libbb.h"
#include "unarchive.h"

int uncompress_main(int argc, char **argv)
{
	const char gunzip_to_stdout = 1;
	const char gunzip_force = 2;
	char status = EXIT_SUCCESS;
	char flags = 0;
	int opt;

	while ((opt = getopt(argc, argv, "cfh")) != -1) {
		switch (opt) {
		case 'c':
			flags |= gunzip_to_stdout;
			break;
		case 'f':
			flags |= gunzip_force;
			break;
		default:
			bb_show_usage();	/* exit's inside usage */
		}
	}

	do {
		struct stat stat_buf;
		const char *old_path = argv[optind];
		const char *delete_path = NULL;
		char *new_path = NULL;
		int src_fd;
		int dst_fd;

		optind++;

		if (old_path == NULL || strcmp(old_path, "-") == 0) {
			src_fd = fileno(stdin);
			flags |= gunzip_to_stdout;
		} else {
			src_fd = bb_xopen(old_path, O_RDONLY);

			/* Get the time stamp on the input file. */
			if (stat(old_path, &stat_buf) < 0) {
				bb_error_msg_and_die("Couldn't stat file %s", old_path);
			}
		}

		/* Check that the input is sane.  */
		if (isatty(src_fd) && ((flags & gunzip_force) == 0)) {
			bb_error_msg_and_die
				("compressed data not read from terminal.  Use -f to force it.");
		}

		/* Set output filename and number */
		if (flags & gunzip_to_stdout) {
			dst_fd = fileno(stdout);
		} else {
			char *extension;

			new_path = bb_xstrdup(old_path);

			extension = strrchr(new_path, '.');
			if (!extension || (strcmp(extension, ".Z") != 0)) {
				bb_error_msg_and_die("Invalid extension");
			}
			*extension = '\0';

			/* Open output file */
			dst_fd = bb_xopen(new_path, O_WRONLY | O_CREAT);

			/* Set permissions on the file */
			chmod(new_path, stat_buf.st_mode);

			/* If unzip succeeds remove the old file */
			delete_path = old_path;
		}

		/* do the decompression, and cleanup */
		if ((bb_xread_char(src_fd) == 0x1f) && (bb_xread_char(src_fd) == 0x9d)) {
			status = uncompress(src_fd, dst_fd);
		} else {
			bb_error_msg_and_die("Invalid magic");
		}

		if ((status != EXIT_SUCCESS) && (new_path)) {
			/* Unzip failed, remove new path instead of old path */
			delete_path = new_path;
		}

		if (dst_fd != fileno(stdout)) {
			close(dst_fd);
		}
		if (src_fd != fileno(stdin)) {
			close(src_fd);
		}

		/* delete_path will be NULL if in test mode or from stdin */
		if (delete_path && (unlink(delete_path) == -1)) {
			bb_error_msg_and_die("Couldn't remove %s", delete_path);
		}

		free(new_path);

	} while (optind < argc);

	return status;
}
