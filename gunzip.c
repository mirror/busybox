/* vi: set sw=4 ts=4: */
/*
 * Gzip implementation for busybox
 *
 * Based on GNU gzip v1.2.4 Copyright (C) 1992-1993 Jean-loup Gailly.
 *
 * Originally adjusted for busybox by Sven Rudolph <sr1@inf.tu-dresden.de>
 * based on gzip sources
 *
 * Adjusted further by Erik Andersen <andersee@debian.org> to support files as
 * well as stdin/stdout, and to generally behave itself wrt command line
 * handling.
 *
 * General cleanup to better adhere to the style guide and make use of standard
 * busybox functions by Glenn McGrath <bug1@optushome.com.au>
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
#include "busybox.h"

const int gunzip_to_stdout = 1;
const int gunzip_force = 2;
const int gunzip_test = 4;
const int gunzip_verbose = 8;

static int gunzip_file (const char *path, int flags)
{
	FILE *in_file, *out_file;
	struct stat stat_buf;
	const char *delete_path = NULL;
	char *out_path = NULL;

	if (path == NULL || strcmp (path, "-") == 0) {
		in_file = stdin;
		flags |= gunzip_to_stdout;
	} else {
		if ((in_file = wfopen(path, "r")) == NULL)
			return -1;

		if (flags & gunzip_verbose) {
			fprintf(stderr, "%s:\t", path);
		}

		/* set the buffer size */
		setvbuf(in_file, NULL, _IOFBF, 0x8000);

		/* Get the time stamp on the input file. */
		if (stat(path, &stat_buf) < 0) {
			error_msg_and_die("Couldn't stat file %s", path);
		}
	}

	/* Check that the input is sane.  */
	if (isatty(fileno(in_file)) && (flags & gunzip_force) == 0)
		error_msg_and_die("compressed data not read from terminal.  Use -f to force it.");

	/* Set output filename and number */
	if (flags & gunzip_test) {
		out_file = xfopen("/dev/null", "w"); /* why does test use filenum 2 ? */
	} else if (flags & gunzip_to_stdout) {
		out_file = stdout;
	} else {
		char *extension;
		int length = strlen(path);

		extension = strrchr(path, '.');
		if (extension && strcmp(extension, ".gz") == 0) {
			length -= 3;
		} else if (extension && strcmp(extension, ".tgz") == 0) {
			length -= 4;
		} else {
			error_msg_and_die("Invalid extension");
		}
		out_path = (char *) xcalloc(sizeof(char), length + 1);
		strncpy(out_path, path, length);

		/* Open output file */
		out_file = xfopen(out_path, "w");

		/* Set permissions on the file */
		chmod(out_path, stat_buf.st_mode);
	}

	/* do the decompression, and cleanup */
	if (unzip(in_file, out_file) == 0) {
		/* Success, remove .gz file */
		if ( !(flags & gunzip_to_stdout ))
			delete_path = path;
		if (flags & gunzip_verbose) {
			fprintf(stderr, "OK\n");
		}
	} else {
		/* remove failed attempt */
		delete_path = out_path;
	}

	fclose(out_file);
	fclose(in_file);

	if (delete_path && !(flags & gunzip_test)) {
		if (unlink(delete_path) < 0) {
			error_msg_and_die("Couldn't remove %s", delete_path);
		}
	}

	free(out_path);

	return 0;
}

extern int gunzip_main(int argc, char **argv)
{
	int flags = 0;
	int i, opt;
	int status = EXIT_SUCCESS;

	/* if called as zcat */
	if (strcmp(applet_name, "zcat") == 0)
		flags |= gunzip_to_stdout;

	while ((opt = getopt(argc, argv, "ctfhdqv")) != -1) {
		switch (opt) {
			case 'c':
				flags |= gunzip_to_stdout;
				break;
			case 'f':
				flags |= gunzip_force;
				break;
			case 't':
				flags |= gunzip_test;
				break;
			case 'v':
				flags |= gunzip_verbose;
				break;
			case 'd': /* Used to convert gzip to gunzip. */
				break;
			case 'q':
				error_msg("-q option not supported, ignored");
				break;
			case 'h':
			default:
				show_usage(); /* exit's inside usage */
		}
	}

	if (optind == argc) {
		if (gunzip_file (NULL, flags) < 0)
			status = EXIT_FAILURE;
	} else
		for (i = optind; i < argc; i++)
			if (gunzip_file (argv[i], flags) < 0)
				status = EXIT_FAILURE;

	return status;
}
