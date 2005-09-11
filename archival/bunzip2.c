/*
 *  Modified for busybox by Glenn McGrath <bug1@iinet.net.au>
 *  Added support output to stdout by Thomas Lundquist <thomasez@zelow.no>
 *
 *  Licensed under GPL v2, see file LICENSE in this tarball for details.
 */

#include <fcntl.h>
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
	char *filename;
	unsigned long opt;
	int status, src_fd, dst_fd;

	opt = bb_getopt_ulflags(argc, argv, "cf");

	/* Set input filename and number */
	filename = argv[optind];
	if ((filename) && (filename[0] != '-') && (filename[1] != '\0')) {
		/* Open input file */
		src_fd = bb_xopen(filename, O_RDONLY);
	} else {
		src_fd = STDIN_FILENO;
		filename = 0;
	}

	/* if called as bzcat force the stdout flag */
	if ((opt & BUNZIP2_OPT_STDOUT) || bb_applet_name[2] == 'c')
		filename = 0;

	/* Check that the input is sane.  */
	if (isatty(src_fd) && (opt & BUNZIP2_OPT_FORCE) == 0) {
		bb_error_msg_and_die("compressed data not read from terminal.  Use -f to force it.");
	}

	if (filename) {
		char *extension=filename+strlen(filename)-4;
		if (strcmp(extension, ".bz2") != 0) {
			bb_error_msg_and_die("Invalid extension");
		}
		*extension=0;
		dst_fd = bb_xopen(filename, O_WRONLY | O_CREAT);
	} else dst_fd = STDOUT_FILENO;
	status = uncompressStream(src_fd, dst_fd);
	if(filename) {
		if (!status) filename[strlen(filename)]='.';
		if (unlink(filename) < 0) {
			bb_error_msg_and_die("Couldn't remove %s", filename);
		}
	}

	return status;
}
/* vi:set ts=4: */
