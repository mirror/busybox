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

static char sha1sum_stream(FILE *fd, unsigned int *hashval)
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

	return(EXIT_SUCCESS);
}

#define FLAG_SILENT	1
#define FLAG_CHECK	2
#define FLAG_WARN	4

static unsigned char *hash_bin_to_hex(unsigned int *hash_value, unsigned char hash_length)
{
	unsigned char x;
	unsigned char *hex_value;

	hex_value = xmalloc(hash_length * 8);
	for (x = 0; x < hash_length; x++) {
		sprintf(&hex_value[x * 8], "%08x", hash_value[x]);
	}
	return(hex_value);
}

FILE *wfopen_file_or_stdin(const char *file_ptr)
{
	FILE *stream;

	if ((file_ptr[0] == '-') && (file_ptr[1] == '\0')) {
		stream = stdin;
	} else {
		stream = bb_wfopen(file_ptr, "r");
	}

	return(stream);
}

/* This could become a common function for md5 as well, by using md5_stream */
extern int authenticate(int argc, char **argv, char (*hash_ptr)(FILE *stream, unsigned int *hashval), const unsigned char hash_length)
{
	unsigned int hash_value[hash_length];
	unsigned int flags;
	int return_value = EXIT_SUCCESS;

#ifdef CONFIG_FEATURE_SHA1SUM_CHECK
	flags = bb_getopt_ulflags(argc, argv, "scw");
#else
	flags = bb_getopt_ulflags(argc, argv, "s");
#endif

#ifdef CONFIG_FEATURE_SHA1SUM_CHECK
	if (!(flags & FLAG_CHECK)) {
		if (flags & FLAG_SILENT) {
			bb_error_msg_and_die("the -s option is meaningful only when verifying checksums");
		}
		else if (flags & FLAG_WARN) {
			bb_error_msg_and_die("the -w option is meaningful only when verifying checksums");
		}
	}
#endif

	if (argc == optind) {
		argv[argc++] = "-";
	}

#ifdef CONFIG_FEATURE_SHA1SUM_CHECK
	if (flags & FLAG_CHECK) {
		FILE *pre_computed_stream;
		int count_total = 0;
		int count_failed = 0;
		unsigned char *file_ptr = argv[optind];

		if (optind + 1 != argc) {
			bb_error_msg_and_die("only one argument may be specified when using -c");
		}
		pre_computed_stream = wfopen_file_or_stdin(file_ptr);
		while (!feof(pre_computed_stream) && !ferror(pre_computed_stream)) {
			FILE *stream;
			char *line;
			char *line_ptr;
			char *hex_value;

			line = bb_get_chomped_line_from_file(pre_computed_stream);
			if (line == NULL) {
				break;
			}
			count_total++;
			line_ptr = strchr(line, ' ');
			if (line_ptr == NULL) {
				if (flags & FLAG_WARN) {
					bb_error_msg("Invalid format");
				}
				free(line);
				continue;
			}
			*line_ptr = '\0';
			line_ptr++;
			if ((flags & FLAG_WARN) && (*line_ptr != ' ')) {
				bb_error_msg("Invalid format");
				free(line);
				continue;
			}
			line_ptr++;
			stream = bb_wfopen(line_ptr, "r");
			if (hash_ptr(stream, hash_value) == EXIT_FAILURE) {
				return_value = EXIT_FAILURE;
			}
			if (fclose(stream) == EOF) {
				bb_perror_msg("Couldnt close file %s", file_ptr);
			}				
			hex_value = hash_bin_to_hex(hash_value, hash_length);
			printf("%s: ", line_ptr);
			if (strcmp(hex_value, line) != 0) {
				puts("FAILED");
				count_failed++;
			} else {
				puts("ok");
			}
			free(line);
		}
		if (count_failed) {
			bb_error_msg("WARNING: %d of %d computed checksum did NOT match", count_failed, count_total);
		}
		if (bb_fclose_nonstdin(pre_computed_stream) == EOF) {
			bb_perror_msg_and_die("Couldnt close file %s", file_ptr);
		}
	} else
#endif
		while (optind < argc) {
			FILE *stream;
			unsigned char *file_ptr = argv[optind];

			optind++;

			stream = wfopen_file_or_stdin(file_ptr);
			if (stream == NULL) {
				return_value = EXIT_FAILURE;
				continue;
			}
			if (hash_ptr(stream, hash_value) == EXIT_FAILURE) {
				return_value = EXIT_FAILURE;
			}
			else if (!flags & FLAG_SILENT) {
				char *hex_value = hash_bin_to_hex(hash_value, hash_length);
				printf("%s  %s\n", hex_value, file_ptr);
				free(hex_value);
			}

			if (bb_fclose_nonstdin(stream) == EOF) {
				bb_perror_msg("Couldnt close file %s", file_ptr);
				return_value = EXIT_FAILURE;
			}
		}

	return(return_value);
}

extern int sha1sum_main(int argc, char **argv)
{
	return (authenticate(argc, argv, sha1sum_stream, 5));
}
