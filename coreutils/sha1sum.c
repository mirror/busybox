/*
 *  Based on shasum from http://www.netsw.org/crypto/hash/
 *  Majorly hacked up to use Dr Brian Gladman's sha1 code
 *
 *  Copyright (C) 1999 Scott G. Miller
 *  Copyright (C) 2003 Glenn L. McGrath
 *  Copyright (C) 2003 Erik Andersen
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
#include <limits.h>
#include <stdint.h>
#include <endian.h>
#include <byteswap.h>
#include "busybox.h"


/*
 ---------------------------------------------------------------------------
 Begin Dr. Gladman's sha1 code
 ---------------------------------------------------------------------------
*/

/*
 ---------------------------------------------------------------------------
 Copyright (c) 2002, Dr Brian Gladman <brg@gladman.me.uk>, Worcester, UK.
 All rights reserved.

 LICENSE TERMS

 The free distribution and use of this software in both source and binary 
 form is allowed (with or without changes) provided that:

   1. distributions of this source code include the above copyright 
      notice, this list of conditions and the following disclaimer;

   2. distributions in binary form include the above copyright
      notice, this list of conditions and the following disclaimer
      in the documentation and/or other associated materials;

   3. the copyright holder's name is not used to endorse products 
      built using this software without specific written permission. 

 ALTERNATIVELY, provided that this notice is retained in full, this product
 may be distributed under the terms of the GNU General Public License (GPL),
 in which case the provisions of the GPL apply INSTEAD OF those given above.
 
 DISCLAIMER

 This software is provided 'as is' with no explicit or implied warranties
 in respect of its properties, including, but not limited to, correctness 
 and/or fitness for purpose.
 ---------------------------------------------------------------------------
 Issue Date: 10/11/2002

 This is a byte oriented version of SHA1 that operates on arrays of bytes
 stored in memory. It runs at 22 cycles per byte on a Pentium P4 processor
*/

#define SHA1_BLOCK_SIZE  64
#define SHA1_DIGEST_SIZE 20
#define SHA1_HASH_SIZE   SHA1_DIGEST_SIZE
#define SHA2_GOOD        0
#define SHA2_BAD         1

/* type to hold the SHA1 context  */
typedef struct
{   uint32_t count[2];
    uint32_t hash[5];
    uint32_t wbuf[16];
} sha1_ctx;

#define rotl32(x,n) (((x) << n) | ((x) >> (32 - n)))

#if __BYTE_ORDER == __BIG_ENDIAN
# define swap_b32(x) (x)
#elif defined(bswap_32)
# define swap_b32(x) bswap_32(x)
#else
# define swap_b32(x) ((rotl32((x), 8) & 0x00ff00ff) | (rotl32((x), 24) & 0xff00ff00))
#endif

#define SHA1_MASK   (SHA1_BLOCK_SIZE - 1)

/* reverse byte order in 32-bit words   */
#define ch(x,y,z)       (((x) & (y)) ^ (~(x) & (z)))
#define parity(x,y,z)   ((x) ^ (y) ^ (z))
#define maj(x,y,z)      (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))

/* A normal version as set out in the FIPS. This version uses   */
/* partial loop unrolling and is optimised for the Pentium 4    */
#define rnd(f,k)    \
    t = a; a = rotl32(a,5) + f(b,c,d) + e + k + w[i]; \
    e = d; d = c; c = rotl32(b, 30); b = t

void sha1_compile(sha1_ctx ctx[1])
{
    uint32_t    w[80], i, a, b, c, d, e, t;

    /* note that words are compiled from the buffer into 32-bit */
    /* words in big-endian order so an order reversal is needed */
    /* here on little endian machines                           */
    for(i = 0; i < SHA1_BLOCK_SIZE / 4; ++i)
        w[i] = swap_b32(ctx->wbuf[i]);

    for(i = SHA1_BLOCK_SIZE / 4; i < 80; ++i)
        w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    a = ctx->hash[0];
    b = ctx->hash[1];
    c = ctx->hash[2];
    d = ctx->hash[3];
    e = ctx->hash[4];

    for(i = 0; i < 20; ++i)
    {
        rnd(ch, 0x5a827999);    
    }

    for(i = 20; i < 40; ++i)
    {
        rnd(parity, 0x6ed9eba1);
    }

    for(i = 40; i < 60; ++i)
    {
        rnd(maj, 0x8f1bbcdc);
    }

    for(i = 60; i < 80; ++i)
    {
        rnd(parity, 0xca62c1d6);
    }

    ctx->hash[0] += a; 
    ctx->hash[1] += b; 
    ctx->hash[2] += c; 
    ctx->hash[3] += d; 
    ctx->hash[4] += e;
}

void sha1_begin(sha1_ctx ctx[1])
{
    ctx->count[0] = ctx->count[1] = 0;
    ctx->hash[0] = 0x67452301;
    ctx->hash[1] = 0xefcdab89;
    ctx->hash[2] = 0x98badcfe;
    ctx->hash[3] = 0x10325476;
    ctx->hash[4] = 0xc3d2e1f0;
}

/* SHA1 hash data in an array of bytes into hash buffer and call the        */
/* hash_compile function as required.                                       */
void sha1_hash(const unsigned char data[], unsigned int len, sha1_ctx ctx[1])
{
    uint32_t pos = (uint32_t)(ctx->count[0] & SHA1_MASK), 
            freeb = SHA1_BLOCK_SIZE - pos;
    const unsigned char *sp = data;

    if((ctx->count[0] += len) < len)
        ++(ctx->count[1]);

    while(len >= freeb)     /* tranfer whole blocks while possible  */
    {
        memcpy(((unsigned char*)ctx->wbuf) + pos, sp, freeb);
        sp += freeb; len -= freeb; freeb = SHA1_BLOCK_SIZE; pos = 0; 
        sha1_compile(ctx);
    }

    memcpy(((unsigned char*)ctx->wbuf) + pos, sp, len);
}

/* SHA1 Final padding and digest calculation  */
#if __BYTE_ORDER == __LITTLE_ENDIAN
static uint32_t  mask[4] = 
	{   0x00000000, 0x000000ff, 0x0000ffff, 0x00ffffff };
static uint32_t  bits[4] = 
	{   0x00000080, 0x00008000, 0x00800000, 0x80000000 };
#else
static uint32_t  mask[4] = 
	{   0x00000000, 0xff000000, 0xffff0000, 0xffffff00 };
static uint32_t  bits[4] = 
	{   0x80000000, 0x00800000, 0x00008000, 0x00000080 };
#endif

void sha1_end(unsigned char hval[], sha1_ctx ctx[1])
{
    uint32_t    i, cnt = (uint32_t)(ctx->count[0] & SHA1_MASK);

    /* mask out the rest of any partial 32-bit word and then set    */
    /* the next byte to 0x80. On big-endian machines any bytes in   */
    /* the buffer will be at the top end of 32 bit words, on little */
    /* endian machines they will be at the bottom. Hence the AND    */
    /* and OR masks above are reversed for little endian systems    */
    ctx->wbuf[cnt >> 2] = (ctx->wbuf[cnt >> 2] & mask[cnt & 3]) | bits[cnt & 3];

    /* we need 9 or more empty positions, one for the padding byte  */
    /* (above) and eight for the length count.  If there is not     */
    /* enough space pad and empty the buffer                        */
    if(cnt > SHA1_BLOCK_SIZE - 9)
    {
        if(cnt < 60) ctx->wbuf[15] = 0;
        sha1_compile(ctx);
        cnt = 0;
    }
    else    /* compute a word index for the empty buffer positions  */
        cnt = (cnt >> 2) + 1;

    while(cnt < 14) /* and zero pad all but last two positions      */ 
        ctx->wbuf[cnt++] = 0;
    
    /* assemble the eight byte counter in the buffer in big-endian  */
    /* format                                                       */

    ctx->wbuf[14] = swap_b32((ctx->count[1] << 3) | (ctx->count[0] >> 29));
    ctx->wbuf[15] = swap_b32(ctx->count[0] << 3);

    sha1_compile(ctx);

    /* extract the hash value as bytes in case the hash buffer is   */
    /* misaligned for 32-bit words                                  */

    for(i = 0; i < SHA1_DIGEST_SIZE; ++i)
        hval[i] = (unsigned char)(ctx->hash[i >> 2] >> 8 * (~i & 3));
}

#if 0
void sha1(unsigned char hval[], const unsigned char data[], unsigned int len)
{   sha1_ctx    cx[1];

    sha1_begin(cx); sha1_hash(data, len, cx); sha1_end(hval, cx);
}
#endif

/*
 ---------------------------------------------------------------------------
 End of Dr. Gladman's sha1 code
 ---------------------------------------------------------------------------
*/

/* Using a larger blocksize can make things _much_ faster
 * by avoiding a zillion tiny little reads */
#define BLOCKSIZE 65536
/* Ensure that BLOCKSIZE is a multiple of 64.  */
#if BLOCKSIZE % SHA1_BLOCK_SIZE != 0
# error "BLOCKSIZE not a multiple of 64"
#endif

static int sha1sum_stream(FILE *stream, unsigned char *hashval)
{
    int result = 0;
    sha1_ctx cx[1];
    size_t sum, n;
    RESERVE_CONFIG_BUFFER(buffer, BLOCKSIZE + 72);

    /* Initialize the computation context.  */
    sha1_begin(cx);

    /* Iterate over full file contents.  */
    while (1) 
    {
	/* We read the file in blocks of BLOCKSIZE bytes.  One call of the
	   computation function processes the whole buffer so that with the
	   next round of the loop another block can be read.  */
	sum = 0;

	/* Read block.  Take care for partial reads.  */
	while (1)
	{
	    n = fread (buffer + sum, 1, BLOCKSIZE - sum, stream);
	    sum += n;

	    if (sum == BLOCKSIZE)
		break;

	    if (n == 0) {
		/* Check for the error flag IFF N == 0, so that we don't
		   exit the loop after a partial read due to e.g., EAGAIN
		   or EWOULDBLOCK.  */
		if (feof (stream)) {
		    sum = 0;
		    goto process_partial_block;
		}
		if (ferror (stream)) {
		    result++;
		    goto all_done;
		}
		goto process_partial_block;
	    }

	    /* We've read at least one byte, so ignore errors.  But always
	       check for EOF, since feof may be true even though N > 0.
	       Otherwise, we could end up calling fread after EOF.  */
	    if (feof (stream))
		goto process_partial_block;
	}

	/* Process buffer */
	sha1_hash(buffer, BLOCKSIZE, cx);
    }

process_partial_block:

    /* Process any remaining bytes.  */
    if (sum > 0)
	sha1_hash(buffer, sum, cx);

    /* Finalize and write the hash into our buffer.  */
    sha1_end(hashval, cx);

all_done:

    RELEASE_CONFIG_BUFFER(buffer);
    return result;
}

#define FLAG_SILENT	1
#define FLAG_CHECK	2
#define FLAG_WARN	4

static unsigned char *hash_bin_to_hex(unsigned char *hash_value, unsigned char hash_length)
{
	int x, len, max;
	unsigned char *hex_value;

	max = (hash_length * 2) + 2;
	hex_value = xmalloc(max);
	for (x = len = 0; x < hash_length; x++) {
	    len += snprintf(hex_value+len, max-len, "%02x", hash_value[x]);
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
extern int authenticate(int argc, char **argv, 
	int (*hash_ptr)(FILE *stream, unsigned char *hashval), 
	const unsigned char hash_length)
{
	unsigned char hash_value[hash_length];
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
				bb_perror_msg("%s", file_ptr);
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
				bb_perror_msg("%s", file_ptr);
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
	return (authenticate(argc, argv, sha1sum_stream, SHA1_HASH_SIZE));
}
