/* vi: set sw=4 ts=4: */
/*
 * Small lzma deflate implementation.
 * Copyright (C) 2006  Aurelien Jacobs <aurel@gnuage.org>
 *
 * Based on bunzip.c from busybox
 *
 * Licensed under GPL v2, see file LICENSE in this tarball for details.
 */

/* Why our g[un]zip/bunzip2 are so ugly compared to this beauty? */

#include "busybox.h"
#include "unarchive.h"

#define UNLZMA_OPT_STDOUT	1

int unlzma_main(int argc, char **argv)
{
	USE_DESKTOP(long long) int status;
	char *filename;
	unsigned opt;
	int src_fd, dst_fd;

	opt = getopt32(argc, argv, "c");

	/* Set input filename and number */
	filename = argv[optind];
	if (filename && NOT_LONE_DASH(filename)) {
		/* Open input file */
		src_fd = xopen(filename, O_RDONLY);
	} else {
		src_fd = STDIN_FILENO;
		filename = 0;
	}

	/* if called as lzmacat force the stdout flag */
	if ((opt & UNLZMA_OPT_STDOUT) || applet_name[4] == 'c')
		filename = 0;

	if (filename) {
		struct stat stat_buf;
		/* bug: char *extension = filename + strlen(filename) - 5; */
		char *extension = strrchr(filename, '.');
		if (!extension || strcmp(extension, ".lzma") != 0) {
			bb_error_msg_and_die("invalid extension");
		}
		xstat(filename, &stat_buf);
		*extension = '\0';
		dst_fd = xopen3(filename, O_WRONLY | O_CREAT | O_TRUNC,
				stat_buf.st_mode);
	} else
		dst_fd = STDOUT_FILENO;
	status = unlzma(src_fd, dst_fd);
	if (filename) {
		if (status >= 0) /* if success delete src, else delete dst */
			filename[strlen(filename)] = '.';
		if (unlink(filename) < 0) {
			bb_error_msg_and_die("cannot remove %s", filename);
		}
	}

	return (status < 0);
}
