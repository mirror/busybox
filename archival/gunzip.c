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
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
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

#if 0
static char *license_msg[] = {
	"   Copyright (C) 1992-1993 Jean-loup Gailly",
	"   This program is free software; you can redistribute it and/or modify",
	"   it under the terms of the GNU General Public License as published by",
	"   the Free Software Foundation; either version 2, or (at your option)",
	"   any later version.",
	"",
	"   This program is distributed in the hope that it will be useful,",
	"   but WITHOUT ANY WARRANTY; without even the implied warranty of",
	"   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the",
	"   GNU General Public License for more details.",
	"",
	"   You should have received a copy of the GNU General Public License",
	"   along with this program; if not, write to the Free Software",
	"   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.",
	0
};
#endif

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "busybox.h"
#include "unarchive.h"

#define GUNZIP_OPT_STDOUT	1
#define GUNZIP_OPT_FORCE	2
#define GUNZIP_OPT_TEST		4
#define GUNZIP_OPT_DECOMPRESS	8

extern int gunzip_main(int argc, char **argv)
{
	char status = EXIT_SUCCESS;
	unsigned long opt;

	opt = bb_getopt_ulflags(argc, argv, "cftd");
	/* if called as zcat */
	if (strcmp(bb_applet_name, "zcat") == 0) {
		opt |= GUNZIP_OPT_STDOUT;
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
			src_fd = STDIN_FILENO;
			opt |= GUNZIP_OPT_STDOUT;
		} else {
			src_fd = bb_xopen(old_path, O_RDONLY);

			/* Get the time stamp on the input file. */
			if (stat(old_path, &stat_buf) < 0) {
				bb_error_msg_and_die("Couldn't stat file %s", old_path);
			}
		}

		/* Check that the input is sane.  */
		if (isatty(src_fd) && ((opt & GUNZIP_OPT_FORCE) == 0)) {
			bb_error_msg_and_die
				("compressed data not read from terminal.  Use -f to force it.");
		}

		/* Set output filename and number */
		if (opt & GUNZIP_OPT_TEST) {
			dst_fd = bb_xopen("/dev/null", O_WRONLY);	/* why does test use filenum 2 ? */
		} else if (opt & GUNZIP_OPT_STDOUT) {
			dst_fd = STDOUT_FILENO;
		} else {
			char *extension;

			new_path = bb_xstrdup(old_path);

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
				bb_error_msg_and_die("Invalid extension");
			}

			/* Open output file */
			dst_fd = bb_xopen(new_path, O_WRONLY | O_CREAT);

			/* Set permissions on the file */
			chmod(new_path, stat_buf.st_mode);

			/* If unzip succeeds remove the old file */
			delete_path = old_path;
		}

		/* do the decompression, and cleanup */
		if (bb_xread_char(src_fd) == 0x1f) {
			unsigned char magic2;

			magic2 = bb_xread_char(src_fd);
#ifdef CONFIG_FEATURE_GUNZIP_UNCOMPRESS
			if (magic2 == 0x9d) {
				status = uncompress(src_fd, dst_fd);
			} else
#endif
				if (magic2 == 0x8b) {
					check_header_gzip(src_fd);
					status = inflate_gunzip(src_fd, dst_fd);
					if (status != 0) {
						bb_error_msg_and_die("Error inflating");
					}
				} else {
					bb_error_msg_and_die("Invalid magic");
				}
		} else {
			bb_error_msg_and_die("Invalid magic");
		}

		if ((status != EXIT_SUCCESS) && (new_path)) {
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
			bb_error_msg_and_die("Couldn't remove %s", delete_path);
		}

		free(new_path);

	} while (optind < argc);

	return status;
}
