/*
 * Small lzma deflate implementation.
 * Copyright (C) 2006  Aurelien Jacobs <aurel@gnuage.org>
 *
 * Based on bunzip.c from busybox
 *
 * Licensed under GPL v2, see file LICENSE in this tarball for details.
 */

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "busybox.h"
#include "unarchive.h"

#define UNLZMA_OPT_STDOUT	1

int unlzma_main(int argc, char **argv)
{
	char *filename;
	unsigned long opt;
	int status, src_fd, dst_fd;

	opt = bb_getopt_ulflags(argc, argv, "c");

	/* Set input filename and number */
	filename = argv[optind];
	if ((filename) && (filename[0] != '-') && (filename[1] != '\0')) {
		/* Open input file */
		src_fd = bb_xopen(filename, O_RDONLY);
	} else {
		src_fd = STDIN_FILENO;
		filename = 0;
	}

	/* if called as lzmacat force the stdout flag */
	if ((opt & UNLZMA_OPT_STDOUT) || bb_applet_name[4] == 'c')
		filename = 0;

	if (filename) {
		char *extension = filename + strlen(filename) - 5;

		if (strcmp(extension, ".lzma") != 0) {
			bb_error_msg_and_die("Invalid extension");
		}
		*extension = 0;
		dst_fd = bb_xopen(filename, O_WRONLY | O_CREAT);
	} else
		dst_fd = STDOUT_FILENO;
	status = unlzma(src_fd, dst_fd);
	if (filename) {
		if (!status)
			filename[strlen(filename)] = '.';
		if (unlink(filename) < 0) {
			bb_error_msg_and_die("Couldn't remove %s", filename);
		}
	}

	return status;
}

/* vi:set ts=4: */
