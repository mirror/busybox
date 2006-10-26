/* vi: set sw=4 ts=4: */
/*
 * cksum - calculate the CRC32 checksum of a file
 *
 * Copyright (C) 2006 by Rob Sullivan, with ideas from code by Walter Harms
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details. */

#include "busybox.h"

int cksum_main(int argc, char **argv)
{

	uint32_t *crc32_table = crc32_filltable(1);

	FILE *fp;
	uint32_t crc;
	long length, filesize;
	int bytes_read;
	char *cp;

	int inp_stdin = (argc == optind) ? 1 : 0;

	do {
		fp = fopen_or_warn_stdin((inp_stdin) ? bb_msg_standard_input : *++argv);

		crc = 0;
		length = 0;

		while ((bytes_read = fread(bb_common_bufsiz1, 1, BUFSIZ, fp)) > 0) {
			cp = bb_common_bufsiz1;
			length += bytes_read;
			while (bytes_read--)
				crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ (*cp++)) & 0xffL];
		}

		filesize = length;

		for (; length; length >>= 8)
			crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ length) & 0xffL];
		crc ^= 0xffffffffL;

		if (inp_stdin) {
			printf("%" PRIu32 " %li\n", crc, filesize);
			break;
		}

		printf("%" PRIu32 " %li %s\n", crc, filesize, *argv);
		fclose(fp);
	} while (*(argv + 1));

	fflush_stdout_and_exit(EXIT_SUCCESS);
}
