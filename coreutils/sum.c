/* vi: set sw=4 ts=4: */
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
 * Licensed under the GPL v2, see the file LICENSE in this tarball.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "busybox.h"

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
	FILE *fp;
	int checksum = 0;          /* The checksum mod 2^16. */
	uintmax_t total_bytes = 0; /* The number of bytes. */
	int ch;                    /* Each character read. */

	if (IS_STDIN(file)) {
		fp = stdin;
		have_read_stdin = 1;
	} else {
		fp = bb_wfopen(file, "r");
		if (fp == NULL)
			return 0;
	}

	while ((ch = getc(fp)) != EOF) {
		++total_bytes;
		checksum = (checksum >> 1) + ((checksum & 1) << 15);
		checksum += ch;
		checksum &= 0xffff;             /* Keep it within bounds. */
	}

	if (ferror(fp)) {
		bb_perror_msg(file);
		bb_fclose_nonstdin(fp);
		return 0;
	}

	if (bb_fclose_nonstdin(fp) == EOF) {
		bb_perror_msg(file);
		return 0;
	}

	printf("%05d %5ju ", checksum, (total_bytes+1023)/1024);
	if (print_name > 1)
		puts(file);
	else
		printf("\n");

	return 1;
}

/* Calculate and print the checksum and the size in 512-byte blocks
   of file FILE, or of the standard input if FILE is "-".
   If PRINT_NAME is >0, print FILE next to the checksum and size.
   Return 1 if successful.  */
#define MY_BUF_SIZE 8192
static int sysv_sum_file(const char *file, int print_name)
{
	RESERVE_CONFIG_UBUFFER(buf, MY_BUF_SIZE);
	int fd;
	uintmax_t total_bytes = 0;

	/* The sum of all the input bytes, modulo (UINT_MAX + 1).  */
	unsigned int s = 0;

	if (IS_STDIN(file)) {
		fd = 0;
		have_read_stdin = 1;
	} else {
		fd = open(file, O_RDONLY);
		if (fd == -1)
			goto release_and_ret;
	}

	while (1) {
		size_t bytes_read = safe_read(fd, buf, MY_BUF_SIZE);

		if (bytes_read == 0)
			break;

		if (bytes_read == -1) {
release_and_ret:
			bb_perror_msg(file);
			RELEASE_CONFIG_BUFFER(buf);
			if (!IS_STDIN(file))
				close(fd);
			return 0;
		}

		total_bytes += bytes_read;
		while (bytes_read--)
			s += buf[bytes_read];
	}

	if (!IS_STDIN(file) && close(fd) == -1)
		goto release_and_ret;
	else
		RELEASE_CONFIG_BUFFER(buf);

	{
		int r = (s & 0xffff) + ((s & 0xffffffff) >> 16);
		s = (r & 0xffff) + (r >> 16);

		printf("%d %ju ", s, (total_bytes+511)/512);
	}
	puts(print_name ? file : "");

	return 1;
}

int sum_main(int argc, char **argv)
{
	int flags;
	int ok;
	int (*sum_func)(const char *, int) = bsd_sum_file;

	/* give the bsd func priority over sysv func */
	flags = bb_getopt_ulflags(argc, argv, "sr");
	if (flags & 1)
		sum_func = sysv_sum_file;
	if (flags & 2)
		sum_func = bsd_sum_file;

	have_read_stdin = 0;
	if ((argc - optind) == 0)
		ok = sum_func("-", 0);
	else
		for (ok = 1; optind < argc; optind++)
			ok &= sum_func(argv[optind], 1);

	if (have_read_stdin && fclose(stdin) == EOF)
		bb_perror_msg_and_die("-");

	exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
}
