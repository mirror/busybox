/* vi: set sw=4 ts=4: */
/*
 *  Copyright (C) 2000 by Glenn McGrath
 *
 *  based on the function base64_encode from http.c in wget v1.6
 *  Copyright (C) 1995, 1996, 1997, 1998, 2000 Free Software Foundation, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "busybox.h"

/* Conversion table.  for base 64 */
static const char tbl_base64[65] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/',
	'=' /* termination character */
};

static const char tbl_std[65] = {
	'`', '!', '"', '#', '$', '%', '&', '\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
	'`' /* termination character */
};

/*
 * Encode the string S of length LENGTH to base64 format and place it
 * to STORE.  STORE will be 0-terminated, and must point to a writable
 * buffer of at least 1+BASE64_LENGTH(length) bytes.
 * where BASE64_LENGTH(len) = (4 * ((LENGTH + 2) / 3))
 */
static void uuencode (const char *s, const char *store, const int length, const char *tbl)
{
	int i;
	unsigned char *p = (unsigned char *)store;

	/* Transform the 3x8 bits to 4x6 bits, as required by base64.  */
	for (i = 0; i < length; i += 3) {
		*p++ = tbl[s[0] >> 2];
		*p++ = tbl[((s[0] & 3) << 4) + (s[1] >> 4)];
		*p++ = tbl[((s[1] & 0xf) << 2) + (s[2] >> 6)];
		*p++ = tbl[s[2] & 0x3f];
		s += 3;
	}
	/* Pad the result if necessary...  */
	if (i == length + 1) {
		*(p - 1) = tbl[64];
	}
	else if (i == length + 2) {
		*(p - 1) = *(p - 2) = tbl[64];
	}
	/* ...and zero-terminate it.  */
	*p = '\0';
}

#define SRC_BUF_SIZE	45  // This *MUST* be a multiple of 3
#define DST_BUF_SIZE    4 * ((SRC_BUF_SIZE + 2) / 3)
int uuencode_main(int argc, char **argv)
{
	const int src_buf_size = SRC_BUF_SIZE;
	const int dst_buf_size = DST_BUF_SIZE;
	int write_size = dst_buf_size;
	struct stat stat_buf;
	FILE *src_stream = stdin;
	const char *tbl;
	size_t size;
	mode_t mode;
	RESERVE_CONFIG_BUFFER(src_buf, SRC_BUF_SIZE + 1);
	RESERVE_CONFIG_BUFFER(dst_buf, DST_BUF_SIZE + 1);

	tbl = tbl_std;
	if (bb_getopt_ulflags(argc, argv, "m") & 1) {
		tbl = tbl_base64;
	}

	switch (argc - optind) {
		case 2:
			src_stream = bb_xfopen(argv[optind], "r");
			if (stat(argv[optind], &stat_buf) < 0) {
				bb_perror_msg_and_die("stat");
			}
			mode = stat_buf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
			if (src_stream == stdout) {
				puts("NULL");
			}
			break;
		case 1:
			mode = 0666 & ~umask(0666);
			break;
		default:
			bb_show_usage();
	}

	bb_printf("begin%s %o %s", tbl == tbl_std ? "" : "-base64", mode, argv[argc - 1]);

	while ((size = fread(src_buf, 1, src_buf_size, src_stream)) > 0) {
		if (size != src_buf_size) {
			/* write_size is always 60 until the last line */
			write_size=(4 * ((size + 2) / 3));
			/* pad with 0s so we can just encode extra bits */
			memset(&src_buf[size], 0, src_buf_size - size);
		}
		/* Encode the buffer we just read in */
		uuencode(src_buf, dst_buf, size, tbl);

		putchar('\n');
		if (tbl == tbl_std) {
			putchar(tbl[size]);
		}
		if (fwrite(dst_buf, 1, write_size, stdout) != write_size) {
			bb_perror_msg_and_die(bb_msg_write_error);
		}
	}
	bb_printf(tbl == tbl_std ? "\n`\nend\n" : "\n====\n");

	bb_xferror(src_stream, "source");	/* TODO - Fix this! */

	bb_fflush_stdout_and_exit(EXIT_SUCCESS);
}
