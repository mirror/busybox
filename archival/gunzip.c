/* vi: set sw=4 ts=4: */
/*
 * Gzip implementation for busybox
 *
 * Based on GNU gzip v1.2.4 Copyright (C) 1992-1993 Jean-loup Gailly.
 *
 * Originally adjusted for busybox by Sven Rudolph <sr1@inf.tu-dresden.de>
 * based on gzip sources
 *
 * Adjusted further by Erik Andersen <andersen@codepoet.org> to support files as
 * well as stdin/stdout, and to generally behave itself wrt command line
 * handling.
 *
 * General cleanup to better adhere to the style guide and make use of standard
 * busybox functions by Glenn McGrath <bug1@iinet.net.au>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * gzip (GNU zip) -- compress files with zip algorithm and 'compress' interface
 * Copyright (C) 1992-1993 Jean-loup Gailly
 * The unzip code was written and put in the public domain by Mark Adler.
 * Portions of the lzw code are derived from the public domain 'compress'
 * written by Spencer Thomas, Joe Orost, James Woods, Jim McKie, Steve Davies,
 * Ken Turkowski, Dave Mack and Peter Jannesen.
 *
 * See the license_msg below and the file COPYING for the software license.
 * See the file algorithm.doc for the compression algorithms and file formats.
 */

#include "busybox.h"
#include "unarchive.h"

#define GUNZIP_OPT_STDOUT	1
#define GUNZIP_OPT_FORCE	2
#define GUNZIP_OPT_TEST		4
#define GUNZIP_OPT_DECOMPRESS	8
#define GUNZIP_OPT_VERBOSE	0x10

int gunzip_main(int argc, char **argv)
{
	USE_DESKTOP(long long) int status;
	int exitcode = 0;
	unsigned opt;

	opt = getopt32(argc, argv, "cftdv");
	/* if called as zcat */
	if (strcmp(applet_name, "zcat") == 0) {
		opt |= GUNZIP_OPT_STDOUT;
	}

	do {
		struct stat stat_buf;
		char *old_path = argv[optind];
		char *delete_path = NULL;
		char *new_path = NULL;
		int src_fd;
		int dst_fd;

		optind++;

		if (old_path == NULL || LONE_DASH(old_path)) {
			src_fd = STDIN_FILENO;
			opt |= GUNZIP_OPT_STDOUT;
			USE_DESKTOP(opt &= ~GUNZIP_OPT_VERBOSE;)
			optind = argc; /* we don't handle "gunzip - a.gz b.gz" */
		} else {
			src_fd = xopen(old_path, O_RDONLY);
			/* Get the time stamp on the input file. */
			fstat(src_fd, &stat_buf);
		}

		/* Check that the input is sane.  */
		if (isatty(src_fd) && !(opt & GUNZIP_OPT_FORCE)) {
			bb_error_msg_and_die
				("compressed data not read from terminal, use -f to force it");
		}

		/* Set output filename and number */
		if (opt & GUNZIP_OPT_TEST) {
			dst_fd = xopen(bb_dev_null, O_WRONLY);	/* why does test use filenum 2 ? */
		} else if (opt & GUNZIP_OPT_STDOUT) {
			dst_fd = STDOUT_FILENO;
		} else {
			char *extension;

			new_path = xstrdup(old_path);

			extension = strrchr(new_path, '.');
#ifdef CONFIG_FEATURE_GUNZIP_UNCOMPRESS
			if (extension && (strcmp(extension, ".Z") == 0)) {
				*extension = '\0';
			} else
#endif
			if (extension && (strcmp(extension, ".gz") == 0)) {
				*extension = '\0';
			} else if (extension && (strcmp(extension, ".tgz") == 0)) {
				extension[2] = 'a';
				extension[3] = 'r';
			} else {
				// FIXME: should we die or just skip to next?
				bb_error_msg_and_die("invalid extension");
			}

			/* Open output file (with correct permissions) */
			dst_fd = xopen3(new_path, O_WRONLY | O_CREAT | O_TRUNC,
					stat_buf.st_mode);

			/* If unzip succeeds remove the old file */
			delete_path = old_path;
		}

		status = -1;
		/* do the decompression, and cleanup */
		if (xread_char(src_fd) == 0x1f) {
			unsigned char magic2;

			magic2 = xread_char(src_fd);
			if (ENABLE_FEATURE_GUNZIP_UNCOMPRESS && magic2 == 0x9d) {
				status = uncompress(src_fd, dst_fd);
			} else if (magic2 == 0x8b) {
				check_header_gzip(src_fd); // FIXME: xfunc? _or_die?
				status = inflate_gunzip(src_fd, dst_fd);
			} else {
				bb_error_msg("invalid magic");
				exitcode = 1;
			}
			if (status < 0) {
				bb_error_msg("error inflating");
				exitcode = 1;
			}
			else if (ENABLE_DESKTOP && (opt & GUNZIP_OPT_VERBOSE)) {
				fprintf(stderr, "%s: %u%% - replaced with %s\n",
					old_path, (unsigned)(stat_buf.st_size*100 / (status+1)), new_path);
			}
		} else {
			bb_error_msg("invalid magic");
			exitcode = 1;
		}
		if (status < 0 && new_path) {
			/* Unzip failed, remove new path instead of old path */
			delete_path = new_path;
		}

		if (dst_fd != STDOUT_FILENO) {
			close(dst_fd);
		}
		if (src_fd != STDIN_FILENO) {
			close(src_fd);
		}

		/* delete_path will be NULL if in test mode or from stdin */
		if (delete_path && (unlink(delete_path) == -1)) {
			bb_error_msg("cannot remove %s", delete_path);
			exitcode = 1;
		}

		free(new_path);

	} while (optind < argc);

	return exitcode;
}
