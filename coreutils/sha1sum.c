/*
 *  Based on shasum from http://www.netsw.org/crypto/hash/
 *  
 *  shasum fixed with reference to coreutils and the nist fip180-1 document
 *  which is incorrect, in section 5
 *  - ft(B,C,D) = (B AND C) OR ((NOT B) AND D) ( 0 <= t <= 19)
 *  + ft(B,C,D) = (D XOR (B AND (C XOR D))) ( 0 <= t <= 19)
 *
 *  Copyright (C) 1999 Scott G. Miller
 *  Copyright (C) 2003 Glenn L. McGrath
 * 
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <endian.h>
#include "busybox.h"

#if __BYTE_ORDER == __BIG_ENDIAN
# define SWAP(n) (n)
#else
# define SWAP(n) \
    (((n) << 24) | (((n) & 0xff00) << 8) | (((n) >> 8) & 0xff00) | ((n) >> 24))
#endif

#define f1(X,Y,Z) (Z ^ (X & (Y ^ Z)))
#define f2(X,Y,Z) (X ^ Y ^ Z)
#define f3(X,Y,Z) ((X & Y) | (Z & (X | Y)))

#define rol1(x) (x<<1) | ((x>>31) & 1)
#define rol5(x) ((x<<5) | ((x>>27) & 0x1f))
#define rol30(x) (x<<30) | ((x>>2) & 0x3fffffff)

static void sha_hash(unsigned int *data, int *hash)
{
	RESERVE_CONFIG_BUFFER(word, 80 * sizeof(unsigned int));
	int *W = (unsigned int *) &word;
	int a = hash[0];
	int b = hash[1];
	int c = hash[2];
	int d = hash[3];
	int e = hash[4];
	int t;
	int TEMP;

	for (t = 0; t < 16; t++) {
		W[t] = SWAP(data[t]);
	}

	/** Data expansion from 16 to 80 blocks **/
	for (t = 16; t < 80; t++) {
		int x = W[t - 3] ^ W[t - 8] ^ W[t - 14] ^ W[t - 16];
		W[t] = rol1(x);
	}

	/** Main loops **/
	for (t = 0; t < 20; t++) {
		TEMP = rol5(a) + f1(b, c, d) + e + W[t] + 0x5a827999;
		e = d;
		d = c;
		c = rol30(b);
		b = a;
		a = TEMP;
	}
	for (; t < 40; t++) {
		TEMP = rol5(a) + f2(b, c, d) + e + W[t] + 0x6ed9eba1;
		e = d;
		d = c;
		c = rol30(b);
		b = a;
		a = TEMP;
	}
	for (; t < 60; t++) {
		TEMP = rol5(a) + f3(b, c, d) + e + W[t] + 0x8f1bbcdc;
		e = d;
		d = c;
		c = rol30(b);
		b = a;
		a = TEMP;
	}
	for (; t < 80; t++) {
		TEMP = rol5(a) + f2(b, c, d) + e + W[t] + 0xca62c1d6;
		e = d;
		d = c;
		c = rol30(b);
		b = a;
		a = TEMP;
	}

	RELEASE_CONFIG_BUFFER(word);

	hash[0] += a;
	hash[1] += b;
	hash[2] += c;
	hash[3] += d;
	hash[4] += e;
}

static void sha1sum_stream(FILE *fd, unsigned int *hashval)
{
	RESERVE_CONFIG_BUFFER(buffer, 64);
	int length = 0;

	hashval[0] = 0x67452301;
	hashval[1] = 0xefcdab89;
	hashval[2] = 0x98badcfe;
	hashval[3] = 0x10325476;
	hashval[4] = 0xc3d2e1f0;

	while (!feof(fd) && !ferror(fd)) {
		int c = fread(&buffer, 1, 64, fd);
		length += c;
		if (feof(fd) || ferror(fd)) {
			int i;
			for (i = c; i < 61; i++) {
				if (i == c) {
					buffer[i] = 0x80;
				}
				else if (i == 60) {
					/* This ends up being swaped twice */
					((unsigned int *) &buffer)[15] = SWAP(length * 8);
				} else {
					buffer[i] = 0;
				}
			}
		}
		sha_hash((unsigned int *) &buffer, hashval);
	}

	RELEASE_CONFIG_BUFFER(buffer);

	return;
}

static void print_hash(unsigned int *hash_value, unsigned char hash_length, unsigned char *filename)
{
	unsigned char x;

	for (x = 0; x < hash_length; x++) {
		printf("%08x", hash_value[x]);
	}
	putchar(' ');
	putchar(' ');
	puts(filename);
}

#define FLAG_SILENT	1
#define FLAG_CHECK	2
#define FLAG_WARN	3

/* This should become a common function used by sha1sum and md5sum,
 * it needs extra functionality first
 */
extern int authenticate(int argc, char **argv, void (*hash_ptr)(FILE *stream, unsigned int *hashval), const unsigned char hash_length)
{
	unsigned int hash_value[hash_length];
	unsigned char flags = 0;
	int opt;

	while ((opt = getopt(argc, argv, "sc:w")) != -1) {
		switch (opt) {
		case 's':	/* Dont output anything, status code shows success */
			flags |= FLAG_SILENT;
			break;
#if 0
		case 'c':	/* Check a list of checksums against stored values  */
			break;
		case 'w':	/* Warn of bad formatting when checking files */
			break;
#endif
		default:
			bb_show_usage();
		}
	}

	if (argc == optind) {
		argv[argc++] = "-";
	}

	while (optind < argc) {
		FILE *stream;
		unsigned char *file_ptr = argv[optind];

		if ((file_ptr[0] == '-') && (file_ptr[1] == '\0')) {
			stream = stdin;
		} else {
			stream = bb_wfopen(file_ptr, "r");
			if (stream == NULL) {
				return(EXIT_FAILURE);
			}
		}
		hash_ptr(stream, hash_value);
		if (!flags & FLAG_SILENT) {
			print_hash(hash_value, hash_length, file_ptr);
		}

		if (fclose(stream) == EOF) {
			bb_perror_msg_and_die("Couldnt close file %s", file_ptr);
		}

		optind++;
	}

	return(EXIT_SUCCESS);
}

extern int sha1sum_main(int argc, char **argv)
{
	return (authenticate(argc, argv, sha1sum_stream, 5));
}
