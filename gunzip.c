/* vi: set sw=4 ts=4: */
/*
 * Gzip implementation for busybox
 *
 * Based on GNU gzip v1.2.4 Copyright (C) 1992-1993 Jean-loup Gailly.
 *
 * Originally adjusted for busybox by Sven Rudolph <sr1@inf.tu-dresden.de>
 * based on gzip sources
 *
 * Adjusted further by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
 * to support files as well as stdin/stdout, and to generally behave itself wrt
 * command line handling.
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

//#include <sys/types.h>
//#include <sys/wait.h>
//#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "busybox.h"
#define BB_DECLARE_EXTERN
#define bb_need_memory_exhausted
#define bb_need_name_too_long
#include "messages.c"

extern int gunzip_main(int argc, char **argv)
{
	FILE *in_file, *out_file;
	struct stat stat_buf;

	char *if_name = NULL;
	char *of_name = NULL;
	char *delete_file_name = NULL;

	const int gunzip_to_stdout = 1;
	const int gunzip_from_stdin = 2;
	const int gunzip_force = 4;
	const int gunzip_test = 8;

	int flags = 0;
	int opt = 0;
	int delete_old_file = FALSE;

	/* if called as zcat */
	if (strcmp(applet_name, "zcat") == 0) {
		if (argc != 2) {
			show_usage();
		}
		optind = 1;
		flags |= (gunzip_force | gunzip_to_stdout);
	} else {
		/* workout flags as regular gunzip */
		/* set default flags */
		if (argc == 1) {
			flags |= (gunzip_from_stdin | gunzip_to_stdout);
		} else {
			/* Parse any options */
			while ((opt = getopt(argc, argv, "ctfh")) != -1) {
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
				case 'h':
				default:
					show_usage(); /* exit's inside usage */
				}
			}
		}
	}

	/* Set input filename and number */
	if (flags & gunzip_from_stdin) {
		in_file = stdin;
		if ((flags & gunzip_force) == 0) {
			error_msg_and_die("data not written to terminal. Use -f to force it.");
		}
	} else {
		if_name = strdup(argv[optind]);
		/* Open input file */
		in_file = xfopen(if_name, "r");

		/* Get the time stamp on the input file. */
		if (stat(if_name, &stat_buf) < 0) {
			error_msg_and_die("Couldnt stat file %s",if_name);
		}
	}

	/* Set output filename and number */
	if (flags & gunzip_to_stdout) {
		out_file = stdout;
		/* whats the best way to do this with streams ? */
		if (isatty(fileno(out_file)) && ((flags & gunzip_force) == 0)) {
			error_msg_and_die("data not written to terminal. Use -f to force it.");
		}
	} else if (flags & gunzip_test) {
		out_file = xfopen("/dev/null", "w"); /* why does test use filenum 2 ? */
	} else {
		char *extension;
		int length = strlen(if_name);

		delete_old_file = TRUE;
		extension = strrchr(if_name, '.');
		if (strcmp(extension, ".gz") == 0) {
			length -= 3;
		} else if (strcmp(extension, ".tgz") == 0) {
			length -= 4;
		} else {
			error_msg_and_die("Invalid extension");
		}
		of_name = (char *) xcalloc(sizeof(char), length + 1);
		strncpy(of_name, if_name, length);

		/* Open output file */
		out_file = xfopen(of_name, "w");

		/* Set permissions on the file */
		chmod(of_name, stat_buf.st_mode);
	}

	/* do the decompression, and cleanup */
	if (unzip(in_file, out_file) == 0) {
		/* Success, remove .gz file */
		delete_file_name = if_name;
	} else {
		/* remove failed attempt */
		delete_file_name = of_name;
	}

	fclose(out_file);
	fclose(in_file);

	if (delete_old_file == TRUE) {
		if (unlink(delete_file_name) < 0) {
			error_msg_and_die("Couldnt remove %s", delete_file_name);
		}
	}

	free(of_name);

	return(EXIT_SUCCESS);
}
