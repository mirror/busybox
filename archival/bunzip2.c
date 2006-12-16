/* vi: set sw=4 ts=4: */
/*
 *  Modified for busybox by Glenn McGrath <bug1@iinet.net.au>
 *  Added support output to stdout by Thomas Lundquist <thomasez@zelow.no>
 *
 *  Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include "unarchive.h"

#define BUNZIP2_OPT_STDOUT	1
#define BUNZIP2_OPT_FORCE	2

int bunzip2_main(int argc, char **argv)
{
	USE_DESKTOP(long long) int status;
	char *filename;
	unsigned opt;
	int src_fd, dst_fd;

	opt = getopt32(argc, argv, "cf");

	/* Set input filename and number */
	filename = argv[optind];
	if (filename && NOT_LONE_DASH(filename)) {
		/* Open input file */
		src_fd = xopen(filename, O_RDONLY);
	} else {
		src_fd = STDIN_FILENO;
		filename = 0;
	}

	/* if called as bzcat force the stdout flag */
	if ((opt & BUNZIP2_OPT_STDOUT) || applet_name[2] == 'c')
		filename = 0;

	/* Check that the input is sane.  */
	if (isatty(src_fd) && (opt & BUNZIP2_OPT_FORCE) == 0) {
		bb_error_msg_and_die("compressed data not read from terminal, "
				"use -f to force it");
	}

	if (filename) {
		struct stat stat_buf;
		/* extension = filename+strlen(filename)-4 is buggy:
		 * strlen may be less than 4 */
		char *extension = strrchr(filename, '.');
		if (!extension || strcmp(extension, ".bz2") != 0) {
			bb_error_msg_and_die("invalid extension");
		}
		xstat(filename, &stat_buf);
		*extension = '\0';
		dst_fd = xopen3(filename, O_WRONLY | O_CREAT | O_TRUNC,
				stat_buf.st_mode);
	} else dst_fd = STDOUT_FILENO;
	status = uncompressStream(src_fd, dst_fd);
	if (filename) {
		if (status >= 0) filename[strlen(filename)] = '.';
		if (unlink(filename) < 0) {
			bb_error_msg_and_die("cannot remove %s", filename);
		}
	}

	return status;
}
