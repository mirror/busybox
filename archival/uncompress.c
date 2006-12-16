/* vi: set sw=4 ts=4: */
/*
 *	Uncompress applet for busybox (c) 2002 Glenn McGrath
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include "unarchive.h"

#define GUNZIP_TO_STDOUT	1
#define GUNZIP_FORCE	2

int uncompress_main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;
	unsigned long flags;

	flags = getopt32(argc, argv, "cf");

	while (optind < argc) {
		char *compressed_file = argv[optind++];
		char *delete_path = NULL;
		char *uncompressed_file = NULL;
		int src_fd;
		int dst_fd;

		if (LONE_DASH(compressed_file)) {
			src_fd = STDIN_FILENO;
			flags |= GUNZIP_TO_STDOUT;
		} else {
			src_fd = xopen(compressed_file, O_RDONLY);
		}

		/* Check that the input is sane.  */
		if (isatty(src_fd) && ((flags & GUNZIP_FORCE) == 0)) {
			bb_error_msg_and_die
				("compressed data not read from terminal.  Use -f to force it.");
		}

		/* Set output filename and number */
		if (flags & GUNZIP_TO_STDOUT) {
			dst_fd = STDOUT_FILENO;
		} else {
			struct stat stat_buf;
			char *extension;

			uncompressed_file = xstrdup(compressed_file);

			extension = strrchr(uncompressed_file, '.');
			if (!extension || (strcmp(extension, ".Z") != 0)) {
				bb_error_msg_and_die("invalid extension");
			}
			*extension = '\0';

			/* Open output file */
			xstat(compressed_file, &stat_buf);
			dst_fd = xopen3(uncompressed_file,
					O_WRONLY | O_CREAT | O_TRUNC,
					stat_buf.st_mode);

			/* If unzip succeeds remove the old file */
			delete_path = compressed_file;
		}

		/* do the decompression, and cleanup */
		if ((xread_char(src_fd) != 0x1f) || (xread_char(src_fd) != 0x9d)) {
			bb_error_msg_and_die("invalid magic");
		}

		status = uncompress(src_fd, dst_fd);

		if ((status != EXIT_SUCCESS) && (uncompressed_file)) {
			/* Unzip failed, remove the uncomressed file instead of compressed file */
			delete_path = uncompressed_file;
		}

		if (dst_fd != STDOUT_FILENO) {
			close(dst_fd);
		}
		if (src_fd != STDIN_FILENO) {
			close(src_fd);
		}

		/* delete_path will be NULL if in test mode or from stdin */
		if (delete_path && (unlink(delete_path) == -1)) {
			bb_error_msg_and_die("cannot remove %s", delete_path);
		}

		free(uncompressed_file);
	}

	return status;
}
