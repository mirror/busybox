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
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "busybox.h"

/* Conversion table.  for base 64 */
static char tbl_base64[64] = {
	'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
	'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
	'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
	'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
	'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
	'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
	'w', 'x', 'y', 'z', '0', '1', '2', '3',
	'4', '5', '6', '7', '8', '9', '+', '/'
};

static char tbl_std[64] = {
	'`', '!', '"', '#', '$', '%', '&', '\'',
	'(', ')', '*', '+', ',', '-', '.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', ':', ';', '<', '=', '>', '?',
	'@', 'A', 'B', 'C', 'D', 'E', 'F', 'G',
	'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W',
	'X', 'Y', 'Z', '[', '\\', ']', '^', '_'
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
		*(p - 1) = '=';
	}
	else if (i == length + 2) {
		*(p - 1) = *(p - 2) = '=';
	}
	/* ...and zero-terminate it.  */
	*p = '\0';
}

int uuencode_main(int argc, char **argv)
{
	const int src_buf_size = 60;	// This *MUST* be a multiple of 3
	const int dst_buf_size = 4 * ((src_buf_size + 2) / 3);
	RESERVE_BB_BUFFER(src_buf, src_buf_size + 1);
	RESERVE_BB_BUFFER(dst_buf, dst_buf_size + 1);
	struct stat stat_buf;
	FILE *src_stream = stdin;
	char *tbl = tbl_std;
	size_t size;
	mode_t mode;
	int opt;
	int column = 0;
	int write_size = 0;
	int remaining;
	int buffer_offset = 0;

	while ((opt = getopt(argc, argv, "m")) != -1) {
		switch (opt) {
		case 'm':
			tbl = tbl_base64;
   			break;
		default:
			show_usage();
		}
	}

	switch (argc - optind) {
		case 2:
			src_stream = xfopen(argv[optind], "r");
			stat(argv[optind], &stat_buf);
			mode = stat_buf.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO);
			if (src_stream == stdout) {
				printf("NULL\n");
			}
			break;
		case 1:
			mode = 0666 & ~umask(0666);
			break;
		default:
			show_usage();
	}

	printf("begin%s %o %s", tbl == tbl_std ? "" : "-base64", mode, argv[argc - 1]);

	while ((size = fread(src_buf, 1, src_buf_size, src_stream)) > 0) {
		/* Encode the buffer we just read in */
		uuencode(src_buf, dst_buf, size, tbl);

		/* Write the buffer to stdout, wrapping at 60 chars.
		 * This looks overly complex, but it gets tricky as
		 * the line has to continue to wrap correctly if we
		 * have to refill the buffer
		 *
		 * Improvments most welcome
		 */

		/* Initialise values for the new buffer */
		remaining = 4 * ((size + 2) / 3);
		buffer_offset = 0;

		/* Write the buffer to stdout, wrapping at 60 chars
		 * starting from the column the last buffer ran out
		 */
		do {
			if (remaining > (60 - column)) {
				write_size = 60 - column;
			}
			else if (remaining < 60) {
				write_size = remaining;
			} else {
				write_size = 60;
			}

			/* Setup a new row if required */
			if (column == 0) {
				putchar('\n');
				if (tbl == tbl_std) {
					putchar('M');
				}
			}
			/* Write to the 60th column */
			if (fwrite(&dst_buf[buffer_offset], 1, write_size, stdout) != write_size) {
				perror("Couldnt finish writing");
			}
			/* Update variables based on last write */
			buffer_offset += write_size;
			remaining -= write_size;
			column += write_size;
			if (column % 60 == 0) {
				column = 0;
			}
		} while (remaining > 0);
	}
	printf(tbl == tbl_std ? "\n`\nend\n" : "\n====\n");

	return(EXIT_SUCCESS);
}
