/*
 *  Based on shasum from http://www.netsw.org/crypto/hash/
 *  
 *  shasum fixed with reference to coreutils and the nist fip180-1 document
 *  which is incorrect, in section 5
 *  - ft(B,C,D) = (B AND C) OR ((NOT B) AND D) ( 0 <= t <= 19)
 *  + ft(B,C,D) = (D XOR (B AND (C XOR S))) ( 0 <= t <= 19)
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

#include "busybox.h"

#ifdef WORDS_BIGENDIAN
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

static void sha1sum_stream(FILE * fd, unsigned int *hashval)
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
			/* If reading from stdin we need to get rid of a tailing character */
			if (fd == stdin) {
				c--;
				length--;
			}
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

static void print_hash(unsigned short hash_length, unsigned int *hash_val, char *filename)
{
	int x;

	for (x = 0; x < hash_length; x++) {
		printf("%08x", hash_val[x]);
	}
	if (filename != NULL) {
		putchar(' ');
		putchar(' ');
		puts(filename);
	}
	putchar('\n');
}

/* This should become a common function used by sha1sum and md5sum,
 * it needs extra functionality first
 */
extern int authenticate(const int argc, char **argv, void (*hash_ptr)(FILE *stream, unsigned int *hashval), const unsigned short hash_length)
{
	int opt;
	unsigned int *hashval;

	while ((opt = getopt(argc, argv, "sc:w")) != -1) {
		switch (opt) {
#if 0
		case 's':	/* Dont output anything, status code shows success */
			break;
		case 'c':	/* Check a list of checksums against stored values  */
			break;
		case 'w':	/* Warn of bad formatting when checking files */
			break;
#endif
		default:
			show_usage();
		}
	}

	hashval = xmalloc(hash_length * sizeof(unsigned int));

	if (argc == optind) {
		hash_ptr(stdin, hashval);
		print_hash(hash_length, hashval, NULL);
	} else {
		int i;

		for (i = optind; i < argc; i++) {
			if (!strcmp(argv[i], "-")) {
				hash_ptr(stdin, hashval);
				print_hash(hash_length, hashval, NULL);
			} else {
				FILE *stream = xfopen(argv[i], "r");
				hash_ptr(stream, hashval);
				fclose(stream);
				print_hash(hash_length, hashval, argv[i]);
			}
		}
	}

	free(hashval);

	return 0;
}

extern int sha1sum_main(int argc, char **argv)
{
	/* sha1 length is 5 nibbles */
	return (authenticate(argc, argv, sha1sum_stream, 5));
}
