/*
 *  Based on shasum from http://www.netsw.org/crypto/hash/
 *  Majorly hacked up to use Dr Brian Gladman's sha1 code
 *
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

#include <byteswap.h>
#include <endian.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "busybox.h"


#ifdef CONFIG_SHA1SUM
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

# define SHA1_BLOCK_SIZE  64
# define SHA1_DIGEST_SIZE 20
# define SHA1_HASH_SIZE   SHA1_DIGEST_SIZE
# define SHA2_GOOD        0
# define SHA2_BAD         1

# define rotl32(x,n) (((x) << n) | ((x) >> (32 - n)))

# if __BYTE_ORDER == __BIG_ENDIAN
#  define swap_b32(x) (x)
# elif defined(bswap_32)
#  define swap_b32(x) bswap_32(x)
# else
#  define swap_b32(x) ((rotl32((x), 8) & 0x00ff00ff) | (rotl32((x), 24) & 0xff00ff00))
# endif /* __BYTE_ORDER */

# define SHA1_MASK   (SHA1_BLOCK_SIZE - 1)

/* reverse byte order in 32-bit words   */
#define ch(x,y,z)       ((z) ^ ((x) & ((y) ^ (z))))
#define parity(x,y,z)   ((x) ^ (y) ^ (z))
#define maj(x,y,z)      (((x) & (y)) | ((z) & ((x) | (y))))

/* A normal version as set out in the FIPS. This version uses   */
/* partial loop unrolling and is optimised for the Pentium 4    */
# define rnd(f,k)    \
    t = a; a = rotl32(a,5) + f(b,c,d) + e + k + w[i]; \
    e = d; d = c; c = rotl32(b, 30); b = t

/* type to hold the SHA1 context  */
struct sha1_ctx_t {
	uint32_t count[2];
	uint32_t hash[5];
	uint32_t wbuf[16];
};

static void sha1_compile(struct sha1_ctx_t *ctx)
{
	uint32_t w[80], i, a, b, c, d, e, t;

	/* note that words are compiled from the buffer into 32-bit */
	/* words in big-endian order so an order reversal is needed */
	/* here on little endian machines                           */
	for (i = 0; i < SHA1_BLOCK_SIZE / 4; ++i)
		w[i] = swap_b32(ctx->wbuf[i]);

	for (i = SHA1_BLOCK_SIZE / 4; i < 80; ++i)
		w[i] = rotl32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

	a = ctx->hash[0];
	b = ctx->hash[1];
	c = ctx->hash[2];
	d = ctx->hash[3];
	e = ctx->hash[4];

	for (i = 0; i < 20; ++i) {
		rnd(ch, 0x5a827999);
	}

	for (i = 20; i < 40; ++i) {
		rnd(parity, 0x6ed9eba1);
	}

	for (i = 40; i < 60; ++i) {
		rnd(maj, 0x8f1bbcdc);
	}

	for (i = 60; i < 80; ++i) {
		rnd(parity, 0xca62c1d6);
	}

	ctx->hash[0] += a;
	ctx->hash[1] += b;
	ctx->hash[2] += c;
	ctx->hash[3] += d;
	ctx->hash[4] += e;
}

static void sha1_begin(struct sha1_ctx_t *ctx)
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
static void sha1_hash(const void *data, size_t len, void *ctx_v)
{
	struct sha1_ctx_t *ctx = (struct sha1_ctx_t *) ctx_v;
	uint32_t pos = (uint32_t) (ctx->count[0] & SHA1_MASK);
	uint32_t freeb = SHA1_BLOCK_SIZE - pos;
	const unsigned char *sp = data;

	if ((ctx->count[0] += len) < len)
		++(ctx->count[1]);

	while (len >= freeb) {	/* tranfer whole blocks while possible  */
		memcpy(((unsigned char *) ctx->wbuf) + pos, sp, freeb);
		sp += freeb;
		len -= freeb;
		freeb = SHA1_BLOCK_SIZE;
		pos = 0;
		sha1_compile(ctx);
	}

	memcpy(((unsigned char *) ctx->wbuf) + pos, sp, len);
}

/* SHA1 Final padding and digest calculation  */
# if __BYTE_ORDER == __LITTLE_ENDIAN
static uint32_t mask[4] = { 0x00000000, 0x000000ff, 0x0000ffff, 0x00ffffff };
static uint32_t bits[4] = { 0x00000080, 0x00008000, 0x00800000, 0x80000000 };
# else
static uint32_t mask[4] = { 0x00000000, 0xff000000, 0xffff0000, 0xffffff00 };
static uint32_t bits[4] = { 0x80000000, 0x00800000, 0x00008000, 0x00000080 };
# endif /* __BYTE_ORDER */

void sha1_end(unsigned char hval[], struct sha1_ctx_t *ctx)
{
	uint32_t i, cnt = (uint32_t) (ctx->count[0] & SHA1_MASK);

	/* mask out the rest of any partial 32-bit word and then set    */
	/* the next byte to 0x80. On big-endian machines any bytes in   */
	/* the buffer will be at the top end of 32 bit words, on little */
	/* endian machines they will be at the bottom. Hence the AND    */
	/* and OR masks above are reversed for little endian systems    */
	ctx->wbuf[cnt >> 2] =
		(ctx->wbuf[cnt >> 2] & mask[cnt & 3]) | bits[cnt & 3];

	/* we need 9 or more empty positions, one for the padding byte  */
	/* (above) and eight for the length count.  If there is not     */
	/* enough space pad and empty the buffer                        */
	if (cnt > SHA1_BLOCK_SIZE - 9) {
		if (cnt < 60)
			ctx->wbuf[15] = 0;
		sha1_compile(ctx);
		cnt = 0;
	} else				/* compute a word index for the empty buffer positions  */
		cnt = (cnt >> 2) + 1;

	while (cnt < 14)	/* and zero pad all but last two positions      */
		ctx->wbuf[cnt++] = 0;

	/* assemble the eight byte counter in the buffer in big-endian  */
	/* format                                                       */

	ctx->wbuf[14] = swap_b32((ctx->count[1] << 3) | (ctx->count[0] >> 29));
	ctx->wbuf[15] = swap_b32(ctx->count[0] << 3);

	sha1_compile(ctx);

	/* extract the hash value as bytes in case the hash buffer is   */
	/* misaligned for 32-bit words                                  */

	for (i = 0; i < SHA1_DIGEST_SIZE; ++i)
		hval[i] = (unsigned char) (ctx->hash[i >> 2] >> 8 * (~i & 3));
}

/*
 ---------------------------------------------------------------------------
 End of Dr. Gladman's sha1 code
 ---------------------------------------------------------------------------
*/
#endif	/* CONFIG_SHA1 */





#ifdef CONFIG_MD5SUM
/*
 * md5sum.c - Compute MD5 checksum of files or strings according to the
 *            definition of MD5 in RFC 1321 from April 1992.
 *
 * Copyright (C) 1995-1999 Free Software Foundation, Inc.
 * Written by Ulrich Drepper <drepper@gnu.ai.mit.edu>, 1995.
 *
 *
 * June 29, 2001        Manuel Novoa III
 *
 * Added MD5SUM_SIZE_VS_SPEED configuration option.
 *
 * Current valid values, with data from my system for comparison, are:
 *   (using uClibc and running on linux-2.4.4.tar.bz2)
 *                     user times (sec)  text size (386)
 *     0 (fastest)         1.1                6144
 *     1                   1.4                5392
 *     2                   3.0                5088
 *     3 (smallest)        5.1                4912
 */

# define MD5SUM_SIZE_VS_SPEED 2

/* Handle endian-ness */
# if __BYTE_ORDER == __LITTLE_ENDIAN
#  define SWAP(n) (n)
# elif defined(bswap_32)
#  define SWAP(n) bswap_32(n)
# else
#  define SWAP(n) ((n << 24) | ((n&65280)<<8) | ((n&16711680)>>8) | (n>>24))
# endif

# if MD5SUM_SIZE_VS_SPEED == 0
/* This array contains the bytes used to pad the buffer to the next
   64-byte boundary.  (RFC 1321, 3.1: Step 1)  */
static const unsigned char fillbuf[64] = { 0x80, 0 /* , 0, 0, ...  */  };
# endif	/* MD5SUM_SIZE_VS_SPEED == 0 */

/* Structure to save state of computation between the single steps.  */
struct md5_ctx_t {
	uint32_t A;
	uint32_t B;
	uint32_t C;
	uint32_t D;
	uint32_t total[2];
	uint32_t buflen;
	char buffer[128];
};

/* Initialize structure containing state of computation.
 * (RFC 1321, 3.3: Step 3)
 */
static void md5_begin(struct md5_ctx_t *ctx)
{
	ctx->A = 0x67452301;
	ctx->B = 0xefcdab89;
	ctx->C = 0x98badcfe;
	ctx->D = 0x10325476;

	ctx->total[0] = ctx->total[1] = 0;
	ctx->buflen = 0;
}

/* These are the four functions used in the four steps of the MD5 algorithm
 * and defined in the RFC 1321.  The first function is a little bit optimized
 * (as found in Colin Plumbs public domain implementation).
 * #define FF(b, c, d) ((b & c) | (~b & d))
 */
# define FF(b, c, d) (d ^ (b & (c ^ d)))
# define FG(b, c, d) FF (d, b, c)
# define FH(b, c, d) (b ^ c ^ d)
# define FI(b, c, d) (c ^ (b | ~d))

/* Starting with the result of former calls of this function (or the
 * initialization function update the context for the next LEN bytes
 * starting at BUFFER.
 * It is necessary that LEN is a multiple of 64!!!
 */
static void md5_hash_block(const void *buffer, size_t len, struct md5_ctx_t *ctx)
{
	uint32_t correct_words[16];
	const uint32_t *words = buffer;
	size_t nwords = len / sizeof(uint32_t);
	const uint32_t *endp = words + nwords;

# if MD5SUM_SIZE_VS_SPEED > 0
	static const uint32_t C_array[] = {
		/* round 1 */
		0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
		0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
		0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
		0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
		/* round 2 */
		0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
		0xd62f105d, 0x2441453, 0xd8a1e681, 0xe7d3fbc8,
		0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
		0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
		/* round 3 */
		0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
		0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
		0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x4881d05,
		0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
		/* round 4 */
		0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
		0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
		0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
		0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
	};

	static const char P_array[] = {
#  if MD5SUM_SIZE_VS_SPEED > 1
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,	/* 1 */
#  endif	/* MD5SUM_SIZE_VS_SPEED > 1 */
		1, 6, 11, 0, 5, 10, 15, 4, 9, 14, 3, 8, 13, 2, 7, 12,	/* 2 */
		5, 8, 11, 14, 1, 4, 7, 10, 13, 0, 3, 6, 9, 12, 15, 2,	/* 3 */
		0, 7, 14, 5, 12, 3, 10, 1, 8, 15, 6, 13, 4, 11, 2, 9	/* 4 */
	};

#  if MD5SUM_SIZE_VS_SPEED > 1
	static const char S_array[] = {
		7, 12, 17, 22,
		5, 9, 14, 20,
		4, 11, 16, 23,
		6, 10, 15, 21
	};
#  endif	/* MD5SUM_SIZE_VS_SPEED > 1 */
# endif

	uint32_t A = ctx->A;
	uint32_t B = ctx->B;
	uint32_t C = ctx->C;
	uint32_t D = ctx->D;

	/* First increment the byte count.  RFC 1321 specifies the possible
	   length of the file up to 2^64 bits.  Here we only compute the
	   number of bytes.  Do a double word increment.  */
	ctx->total[0] += len;
	if (ctx->total[0] < len)
		++ctx->total[1];

	/* Process all bytes in the buffer with 64 bytes in each round of
	   the loop.  */
	while (words < endp) {
		uint32_t *cwp = correct_words;
		uint32_t A_save = A;
		uint32_t B_save = B;
		uint32_t C_save = C;
		uint32_t D_save = D;

# if MD5SUM_SIZE_VS_SPEED > 1
#  define CYCLIC(w, s) (w = (w << s) | (w >> (32 - s)))

		const uint32_t *pc;
		const char *pp;
		const char *ps;
		int i;
		uint32_t temp;

		for (i = 0; i < 16; i++) {
			cwp[i] = SWAP(words[i]);
		}
		words += 16;

#  if MD5SUM_SIZE_VS_SPEED > 2
		pc = C_array;
		pp = P_array;
		ps = S_array - 4;

		for (i = 0; i < 64; i++) {
			if ((i & 0x0f) == 0)
				ps += 4;
			temp = A;
			switch (i >> 4) {
			case 0:
				temp += FF(B, C, D);
				break;
			case 1:
				temp += FG(B, C, D);
				break;
			case 2:
				temp += FH(B, C, D);
				break;
			case 3:
				temp += FI(B, C, D);
			}
			temp += cwp[(int) (*pp++)] + *pc++;
			CYCLIC(temp, ps[i & 3]);
			temp += B;
			A = D;
			D = C;
			C = B;
			B = temp;
		}
#  else
		pc = C_array;
		pp = P_array;
		ps = S_array;

		for (i = 0; i < 16; i++) {
			temp = A + FF(B, C, D) + cwp[(int) (*pp++)] + *pc++;
			CYCLIC(temp, ps[i & 3]);
			temp += B;
			A = D;
			D = C;
			C = B;
			B = temp;
		}

		ps += 4;
		for (i = 0; i < 16; i++) {
			temp = A + FG(B, C, D) + cwp[(int) (*pp++)] + *pc++;
			CYCLIC(temp, ps[i & 3]);
			temp += B;
			A = D;
			D = C;
			C = B;
			B = temp;
		}
		ps += 4;
		for (i = 0; i < 16; i++) {
			temp = A + FH(B, C, D) + cwp[(int) (*pp++)] + *pc++;
			CYCLIC(temp, ps[i & 3]);
			temp += B;
			A = D;
			D = C;
			C = B;
			B = temp;
		}
		ps += 4;
		for (i = 0; i < 16; i++) {
			temp = A + FI(B, C, D) + cwp[(int) (*pp++)] + *pc++;
			CYCLIC(temp, ps[i & 3]);
			temp += B;
			A = D;
			D = C;
			C = B;
			B = temp;
		}

#  endif	/* MD5SUM_SIZE_VS_SPEED > 2 */
# else
		/* First round: using the given function, the context and a constant
		   the next context is computed.  Because the algorithms processing
		   unit is a 32-bit word and it is determined to work on words in
		   little endian byte order we perhaps have to change the byte order
		   before the computation.  To reduce the work for the next steps
		   we store the swapped words in the array CORRECT_WORDS.  */

#  define OP(a, b, c, d, s, T)	\
      do	\
        {	\
	  a += FF (b, c, d) + (*cwp++ = SWAP (*words)) + T;	\
	  ++words;	\
	  CYCLIC (a, s);	\
	  a += b;	\
        }	\
      while (0)

		/* It is unfortunate that C does not provide an operator for
		   cyclic rotation.  Hope the C compiler is smart enough.  */
		/* gcc 2.95.4 seems to be --aaronl */
#  define CYCLIC(w, s) (w = (w << s) | (w >> (32 - s)))

		/* Before we start, one word to the strange constants.
		   They are defined in RFC 1321 as

		   T[i] = (int) (4294967296.0 * fabs (sin (i))), i=1..64
		 */

#  if MD5SUM_SIZE_VS_SPEED == 1
		const uint32_t *pc;
		const char *pp;
		int i;
#  endif	/* MD5SUM_SIZE_VS_SPEED */

		/* Round 1.  */
#  if MD5SUM_SIZE_VS_SPEED == 1
		pc = C_array;
		for (i = 0; i < 4; i++) {
			OP(A, B, C, D, 7, *pc++);
			OP(D, A, B, C, 12, *pc++);
			OP(C, D, A, B, 17, *pc++);
			OP(B, C, D, A, 22, *pc++);
		}
#  else
		OP(A, B, C, D, 7, 0xd76aa478);
		OP(D, A, B, C, 12, 0xe8c7b756);
		OP(C, D, A, B, 17, 0x242070db);
		OP(B, C, D, A, 22, 0xc1bdceee);
		OP(A, B, C, D, 7, 0xf57c0faf);
		OP(D, A, B, C, 12, 0x4787c62a);
		OP(C, D, A, B, 17, 0xa8304613);
		OP(B, C, D, A, 22, 0xfd469501);
		OP(A, B, C, D, 7, 0x698098d8);
		OP(D, A, B, C, 12, 0x8b44f7af);
		OP(C, D, A, B, 17, 0xffff5bb1);
		OP(B, C, D, A, 22, 0x895cd7be);
		OP(A, B, C, D, 7, 0x6b901122);
		OP(D, A, B, C, 12, 0xfd987193);
		OP(C, D, A, B, 17, 0xa679438e);
		OP(B, C, D, A, 22, 0x49b40821);
#  endif	/* MD5SUM_SIZE_VS_SPEED == 1 */

		/* For the second to fourth round we have the possibly swapped words
		   in CORRECT_WORDS.  Redefine the macro to take an additional first
		   argument specifying the function to use.  */
#  undef OP
#  define OP(f, a, b, c, d, k, s, T)	\
      do	\
	{	\
	  a += f (b, c, d) + correct_words[k] + T;	\
	  CYCLIC (a, s);	\
	  a += b;	\
	}	\
      while (0)

		/* Round 2.  */
#  if MD5SUM_SIZE_VS_SPEED == 1
		pp = P_array;
		for (i = 0; i < 4; i++) {
			OP(FG, A, B, C, D, (int) (*pp++), 5, *pc++);
			OP(FG, D, A, B, C, (int) (*pp++), 9, *pc++);
			OP(FG, C, D, A, B, (int) (*pp++), 14, *pc++);
			OP(FG, B, C, D, A, (int) (*pp++), 20, *pc++);
		}
#  else
		OP(FG, A, B, C, D, 1, 5, 0xf61e2562);
		OP(FG, D, A, B, C, 6, 9, 0xc040b340);
		OP(FG, C, D, A, B, 11, 14, 0x265e5a51);
		OP(FG, B, C, D, A, 0, 20, 0xe9b6c7aa);
		OP(FG, A, B, C, D, 5, 5, 0xd62f105d);
		OP(FG, D, A, B, C, 10, 9, 0x02441453);
		OP(FG, C, D, A, B, 15, 14, 0xd8a1e681);
		OP(FG, B, C, D, A, 4, 20, 0xe7d3fbc8);
		OP(FG, A, B, C, D, 9, 5, 0x21e1cde6);
		OP(FG, D, A, B, C, 14, 9, 0xc33707d6);
		OP(FG, C, D, A, B, 3, 14, 0xf4d50d87);
		OP(FG, B, C, D, A, 8, 20, 0x455a14ed);
		OP(FG, A, B, C, D, 13, 5, 0xa9e3e905);
		OP(FG, D, A, B, C, 2, 9, 0xfcefa3f8);
		OP(FG, C, D, A, B, 7, 14, 0x676f02d9);
		OP(FG, B, C, D, A, 12, 20, 0x8d2a4c8a);
#  endif	/* MD5SUM_SIZE_VS_SPEED == 1 */

		/* Round 3.  */
#  if MD5SUM_SIZE_VS_SPEED == 1
		for (i = 0; i < 4; i++) {
			OP(FH, A, B, C, D, (int) (*pp++), 4, *pc++);
			OP(FH, D, A, B, C, (int) (*pp++), 11, *pc++);
			OP(FH, C, D, A, B, (int) (*pp++), 16, *pc++);
			OP(FH, B, C, D, A, (int) (*pp++), 23, *pc++);
		}
#  else
		OP(FH, A, B, C, D, 5, 4, 0xfffa3942);
		OP(FH, D, A, B, C, 8, 11, 0x8771f681);
		OP(FH, C, D, A, B, 11, 16, 0x6d9d6122);
		OP(FH, B, C, D, A, 14, 23, 0xfde5380c);
		OP(FH, A, B, C, D, 1, 4, 0xa4beea44);
		OP(FH, D, A, B, C, 4, 11, 0x4bdecfa9);
		OP(FH, C, D, A, B, 7, 16, 0xf6bb4b60);
		OP(FH, B, C, D, A, 10, 23, 0xbebfbc70);
		OP(FH, A, B, C, D, 13, 4, 0x289b7ec6);
		OP(FH, D, A, B, C, 0, 11, 0xeaa127fa);
		OP(FH, C, D, A, B, 3, 16, 0xd4ef3085);
		OP(FH, B, C, D, A, 6, 23, 0x04881d05);
		OP(FH, A, B, C, D, 9, 4, 0xd9d4d039);
		OP(FH, D, A, B, C, 12, 11, 0xe6db99e5);
		OP(FH, C, D, A, B, 15, 16, 0x1fa27cf8);
		OP(FH, B, C, D, A, 2, 23, 0xc4ac5665);
#  endif	/* MD5SUM_SIZE_VS_SPEED == 1 */

		/* Round 4.  */
#  if MD5SUM_SIZE_VS_SPEED == 1
		for (i = 0; i < 4; i++) {
			OP(FI, A, B, C, D, (int) (*pp++), 6, *pc++);
			OP(FI, D, A, B, C, (int) (*pp++), 10, *pc++);
			OP(FI, C, D, A, B, (int) (*pp++), 15, *pc++);
			OP(FI, B, C, D, A, (int) (*pp++), 21, *pc++);
		}
#  else
		OP(FI, A, B, C, D, 0, 6, 0xf4292244);
		OP(FI, D, A, B, C, 7, 10, 0x432aff97);
		OP(FI, C, D, A, B, 14, 15, 0xab9423a7);
		OP(FI, B, C, D, A, 5, 21, 0xfc93a039);
		OP(FI, A, B, C, D, 12, 6, 0x655b59c3);
		OP(FI, D, A, B, C, 3, 10, 0x8f0ccc92);
		OP(FI, C, D, A, B, 10, 15, 0xffeff47d);
		OP(FI, B, C, D, A, 1, 21, 0x85845dd1);
		OP(FI, A, B, C, D, 8, 6, 0x6fa87e4f);
		OP(FI, D, A, B, C, 15, 10, 0xfe2ce6e0);
		OP(FI, C, D, A, B, 6, 15, 0xa3014314);
		OP(FI, B, C, D, A, 13, 21, 0x4e0811a1);
		OP(FI, A, B, C, D, 4, 6, 0xf7537e82);
		OP(FI, D, A, B, C, 11, 10, 0xbd3af235);
		OP(FI, C, D, A, B, 2, 15, 0x2ad7d2bb);
		OP(FI, B, C, D, A, 9, 21, 0xeb86d391);
#  endif	/* MD5SUM_SIZE_VS_SPEED == 1 */
# endif	/* MD5SUM_SIZE_VS_SPEED > 1 */

		/* Add the starting values of the context.  */
		A += A_save;
		B += B_save;
		C += C_save;
		D += D_save;
	}

	/* Put checksum in context given as argument.  */
	ctx->A = A;
	ctx->B = B;
	ctx->C = C;
	ctx->D = D;
}

/* Starting with the result of former calls of this function (or the
 * initialization function update the context for the next LEN bytes
 * starting at BUFFER.
 * It is NOT required that LEN is a multiple of 64.
 */

static void md5_hash_bytes(const void *buffer, size_t len, struct md5_ctx_t *ctx)
{
	/* When we already have some bits in our internal buffer concatenate
	   both inputs first.  */
	if (ctx->buflen != 0) {
		size_t left_over = ctx->buflen;
		size_t add = 128 - left_over > len ? len : 128 - left_over;

		memcpy(&ctx->buffer[left_over], buffer, add);
		ctx->buflen += add;

		if (left_over + add > 64) {
			md5_hash_block(ctx->buffer, (left_over + add) & ~63, ctx);
			/* The regions in the following copy operation cannot overlap.  */
			memcpy(ctx->buffer, &ctx->buffer[(left_over + add) & ~63],
				   (left_over + add) & 63);
			ctx->buflen = (left_over + add) & 63;
		}

		buffer = (const char *) buffer + add;
		len -= add;
	}

	/* Process available complete blocks.  */
	if (len > 64) {
		md5_hash_block(buffer, len & ~63, ctx);
		buffer = (const char *) buffer + (len & ~63);
		len &= 63;
	}

	/* Move remaining bytes in internal buffer.  */
	if (len > 0) {
		memcpy(ctx->buffer, buffer, len);
		ctx->buflen = len;
	}
}

static void md5_hash(const void *buffer, size_t length, void *md5_ctx)
{
	if (length % 64 == 0) {
		md5_hash_block(buffer, length, md5_ctx);
	} else {
		md5_hash_bytes(buffer, length, md5_ctx);
	}
}

/* Process the remaining bytes in the buffer and put result from CTX
 * in first 16 bytes following RESBUF.  The result is always in little
 * endian byte order, so that a byte-wise output yields to the wanted
 * ASCII representation of the message digest.
 *
 * IMPORTANT: On some systems it is required that RESBUF is correctly
 * aligned for a 32 bits value.
 */
static void *md5_end(void *resbuf, struct md5_ctx_t *ctx)
{
	/* Take yet unprocessed bytes into account.  */
	uint32_t bytes = ctx->buflen;
	size_t pad;

	/* Now count remaining bytes.  */
	ctx->total[0] += bytes;
	if (ctx->total[0] < bytes)
		++ctx->total[1];

	pad = bytes >= 56 ? 64 + 56 - bytes : 56 - bytes;
# if MD5SUM_SIZE_VS_SPEED > 0
	memset(&ctx->buffer[bytes], 0, pad);
	ctx->buffer[bytes] = 0x80;
# else
	memcpy(&ctx->buffer[bytes], fillbuf, pad);
# endif	/* MD5SUM_SIZE_VS_SPEED > 0 */

	/* Put the 64-bit file length in *bits* at the end of the buffer.  */
	*(uint32_t *) & ctx->buffer[bytes + pad] = SWAP(ctx->total[0] << 3);
	*(uint32_t *) & ctx->buffer[bytes + pad + 4] =
		SWAP(((ctx->total[1] << 3) | (ctx->total[0] >> 29)));

	/* Process last bytes.  */
	md5_hash_block(ctx->buffer, bytes + pad + 8, ctx);

	/* Put result from CTX in first 16 bytes following RESBUF.  The result is
	 * always in little endian byte order, so that a byte-wise output yields
	 * to the wanted ASCII representation of the message digest.
	 *
	 * IMPORTANT: On some systems it is required that RESBUF is correctly
	 * aligned for a 32 bits value.
	 */
	((uint32_t *) resbuf)[0] = SWAP(ctx->A);
	((uint32_t *) resbuf)[1] = SWAP(ctx->B);
	((uint32_t *) resbuf)[2] = SWAP(ctx->C);
	((uint32_t *) resbuf)[3] = SWAP(ctx->D);

	return resbuf;
}
#endif	/* CONFIG_MD5SUM */




extern int hash_fd(int src_fd, const size_t size, const uint8_t hash_algo,
				   uint8_t * hashval)
{
	int result = EXIT_SUCCESS;
//	size_t hashed_count = 0;
	size_t blocksize = 0;
	size_t remaining = size;
	unsigned char *buffer = NULL;
	void (*hash_fn_ptr)(const void *, size_t, void *) = NULL;
	void *cx = NULL;

#ifdef CONFIG_SHA1SUM
	struct sha1_ctx_t sha1_cx;
#endif
#ifdef CONFIG_MD5SUM
	struct md5_ctx_t md5_cx;
#endif


#ifdef CONFIG_SHA1SUM
	if (hash_algo == HASH_SHA1) {
		/* Ensure that BLOCKSIZE is a multiple of 64.  */
		blocksize = 65536;
		buffer = xmalloc(blocksize);
		hash_fn_ptr = sha1_hash;
		cx = &sha1_cx;
	}
#endif
#ifdef CONFIG_MD5SUM
	if (hash_algo == HASH_MD5) {
		blocksize = 4096;
		buffer = xmalloc(blocksize + 72);
		hash_fn_ptr = md5_hash;
		cx = &md5_cx;
	}
#endif
	
	/* Initialize the computation context.  */
#ifdef CONFIG_SHA1SUM
	if (hash_algo == HASH_SHA1) {
		sha1_begin(&sha1_cx);
	}
#endif
#ifdef CONFIG_MD5SUM
	if (hash_algo == HASH_MD5) {
		md5_begin(&md5_cx);
	}
#endif
	/* Iterate over full file contents.  */
	while ((remaining == (size_t) -1) || (remaining > 0)) {
		size_t read_try;
		ssize_t read_got;

		if (remaining > blocksize) {
			read_try = blocksize;
		} else {
			read_try = remaining;
		}
		read_got = bb_full_read(src_fd, buffer, read_try);
		if (read_got < 1) {
			/* count == 0 means short read
			 * count == -1 means read error */
			result = read_got - 1;
			break;
		}
		if (remaining != (size_t) -1) {
			remaining -= read_got;
		}

		/* Process buffer */
		hash_fn_ptr(buffer, read_got, cx);
	}

	/* Finalize and write the hash into our buffer.  */
#ifdef CONFIG_SHA1SUM
	if (hash_algo == HASH_SHA1) {
		sha1_end(hashval, &sha1_cx);
	}
#endif
#ifdef CONFIG_MD5SUM
	if (hash_algo == HASH_MD5) {
		md5_end(hashval, &md5_cx);
	}
#endif

	free(buffer);
	return result;
}
