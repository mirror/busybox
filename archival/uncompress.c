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

#define GUNZIP_TO_STDOUT	1
#define GUNZIP_FORCE	2

extern int uncompress_main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;
	unsigned long flags;

	flags = bb_getopt_ulflags(argc, argv, "cf");

	while (optind < argc) {
		const char *compressed_file = argv[optind++];
		const char *delete_path = NULL;
		char *uncompressed_file = NULL;
		int src_fd;
		int dst_fd;

		if (strcmp(compressed_file, "-") == 0) {
			src_fd = fileno(stdin);
			flags |= GUNZIP_TO_STDOUT;
		} else {
			src_fd = bb_xopen(compressed_file, O_RDONLY);
		}

		/* Check that the input is sane.  */
		if (isatty(src_fd) && ((flags & GUNZIP_FORCE) == 0)) {
			bb_error_msg_and_die
				("compressed data not read from terminal.  Use -f to force it.");
		}

		/* Set output filename and number */
		if (flags & GUNZIP_TO_STDOUT) {
			dst_fd = fileno(stdout);
		} else {
			struct stat stat_buf;
			char *extension;

			uncompressed_file = bb_xstrdup(compressed_file);

			extension = strrchr(uncompressed_file, '.');
			if (!extension || (strcmp(extension, ".Z") != 0)) {
				bb_error_msg_and_die("Invalid extension");
			}
			*extension = '\0';

			/* Open output file */
			dst_fd = bb_xopen(uncompressed_file, O_WRONLY | O_CREAT);

			/* Set permissions on the file */
			stat(compressed_file, &stat_buf);
			chmod(uncompressed_file, stat_buf.st_mode);

			/* If unzip succeeds remove the old file */
			delete_path = compressed_file;
		}

		/* do the decompression, and cleanup */
		if ((bb_xread_char(src_fd) != 0x1f) || (bb_xread_char(src_fd) != 0x9d)) {
			bb_error_msg_and_die("Invalid magic");
		}

		status = uncompress(src_fd, dst_fd);

		if ((status != EXIT_SUCCESS) && (uncompressed_file)) {
			/* Unzip failed, remove the uncomressed file instead of compressed file */
			delete_path = uncompressed_file;
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

		free(uncompressed_file);
	}

	return status;
}
