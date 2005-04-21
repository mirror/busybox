/*
 * sum -- checksum and count the blocks in a file
 *     Like BSD sum or SysV sum -r, except like SysV sum if -s option is given.
 *
 * Copyright (C) 86, 89, 91, 1995-2002, 2004 Free Software Foundation, Inc.
 * Copyright (C) 2005 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2005 by Mike Frysinger <vapier@gentoo.org>
 *
 * Written by Kayvan Aghaiepour and David MacKenzie
 * Taken from coreutils and turned into a busybox applet by Mike Frysinger
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
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <getopt.h>

#include "libbb.h"

/* 1 if any of the files read were the standard input */
static int have_read_stdin;

/* make a little more readable and avoid using strcmp for just 2 bytes */
#define IS_STDIN(s) (s[0] == '-' && s[1] == '\0')

/* Calculate and print the rotated checksum and the size in 1K blocks
   of file FILE, or of the standard input if FILE is "-".
   If PRINT_NAME is >1, print FILE next to the checksum and size.
   The checksum varies depending on sizeof (int).
   Return 1 if successful.  */
static int bsd_sum_file(const char *file, int print_name)
{
	register FILE *fp;
	register int checksum = 0;          /* The checksum mod 2^16. */
	register uintmax_t total_bytes = 0; /* The number of bytes. */
	register int ch;                    /* Each character read. */

	if (IS_STDIN(file)) {
		fp = stdin;
		have_read_stdin = 1;
	} else {
		fp = fopen(file, "r");
		if (fp == NULL) {
			bb_perror_msg("%s", file);
			return 0;
		}
	}

	while ((ch = getc(fp)) != EOF) {
		++total_bytes;
		checksum = (checksum >> 1) + ((checksum & 1) << 15);
		checksum += ch;
		checksum &= 0xffff;             /* Keep it within bounds. */
	}

	if (ferror(fp)) {
		bb_perror_msg("%s", file);
		if (!IS_STDIN(file))
			fclose(fp);
		return 0;
	}

	if (!IS_STDIN(file) && fclose(fp) == EOF) {
		bb_perror_msg("%s", file);
		return 0;
	}

	printf("%05d %5s", checksum,
	       make_human_readable_str(total_bytes, 1, 1024));
	if (print_name > 1)
		printf(" %s", file);
	putchar('\n');

	return 1;
}

/* Calculate and print the checksum and the size in 512-byte blocks
   of file FILE, or of the standard input if FILE is "-".
   If PRINT_NAME is >0, print FILE next to the checksum and size.
   Return 1 if successful.  */
static int sysv_sum_file(const char *file, int print_name)
{
	int fd;
	unsigned char buf[8192];
	uintmax_t total_bytes = 0;
	int r;
	int checksum;

	/* The sum of all the input bytes, modulo (UINT_MAX + 1).  */
	unsigned int s = 0;

	if (IS_STDIN(file)) {
		fd = 0;
		have_read_stdin = 1;
	} else {
		fd = open(file, O_RDONLY);
		if (fd == -1) {
			bb_perror_msg("%s", file);
			return 0;
		}
	}

	while (1) {
		size_t i;
		size_t bytes_read = safe_read(fd, buf, sizeof(buf));

		if (bytes_read == 0)
			break;

		if (bytes_read == -1) {
			bb_perror_msg("%s", file);
			if (!IS_STDIN(file))
				close(fd);
			return 0;
		}

		for (i = 0; i < bytes_read; i++)
			s += buf[i];
		total_bytes += bytes_read;
	}

	if (!IS_STDIN(file) && close(fd) == -1) {
		bb_perror_msg("%s", file);
		return 0;
	}

	r = (s & 0xffff) + ((s & 0xffffffff) >> 16);
	checksum = (r & 0xffff) + (r >> 16);

	printf("%d %s", checksum,
	       make_human_readable_str(total_bytes, 1, 512));
	if (print_name)
		printf(" %s", file);
	putchar('\n');

	return 1;
}

int sum_main(int argc, char **argv)
{
	int flags;
	int ok;
	int files_given;
	int (*sum_func)(const char *, int) = bsd_sum_file;

	/* give the bsd func priority over sysv func */
	flags = bb_getopt_ulflags(argc, argv, "sr");
	if (flags & 1)
		sum_func = sysv_sum_file;
	if (flags & 2)
		sum_func = bsd_sum_file;

	have_read_stdin = 0;
	files_given = argc - optind;
	if (files_given <= 0)
		ok = sum_func("-", files_given);
	else
		for (ok = 1; optind < argc; optind++)
			ok &= sum_func(argv[optind], files_given);

	if (have_read_stdin && fclose(stdin) == EOF)
		bb_perror_msg_and_die("-");

	exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
