/* vi: set sw=4 ts=4: */
/*
 * Based on shasum from http://www.netsw.org/crypto/hash/
 * Majorly hacked up to use Dr Brian Gladman's sha1 code
 *
 * Copyright (C) 2002 Dr Brian Gladman <brg@gladman.me.uk>, Worcester, UK.
 * Copyright (C) 2003 Glenn L. McGrath
 * Copyright (C) 2003 Erik Andersen
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 *
 * ---------------------------------------------------------------------------
 * Issue Date: 10/11/2002
 *
 * This is a byte oriented version of SHA1 that operates on arrays of bytes
 * stored in memory. It runs at 22 cycles per byte on a Pentium P4 processor
 *
 * ---------------------------------------------------------------------------
 *
 * SHA256 and SHA512 parts are:
 * Released into the Public Domain by Ulrich Drepper <drepper@redhat.com>.
 * TODO: shrink them.
 */

#include "libbb.h"

#define rotl32(x,n) (((x) << (n)) | ((x) >> (32 - (n))))
#define rotr32(x,n) (((x) >> (n)) | ((x) << (32 - (n))))
/* for sha512: */
#define rotr64(x,n) (((x) >> (n)) | ((x) << (64 - (n))))
#if BB_LITTLE_ENDIAN
static inline uint64_t hton64(uint64_t v)
{
	return (((uint64_t)htonl(v)) << 32) | htonl(v >> 32);
}
#else
#define hton64(v) (v)
#endif
#define ntoh64(v) hton64(v)

/* To check alignment gcc has an appropriate operator.  Other
   compilers don't.  */
#if defined(__GNUC__) && __GNUC__ >= 2
# define UNALIGNED_P(p,type) (((uintptr_t) p) % __alignof__(type) != 0)
#else
# define UNALIGNED_P(p,type) (((uintptr_t) p) % sizeof(type) != 0)
#endif


#define SHA1_BLOCK_SIZE  64
#define SHA1_DIGEST_SIZE 20
#define SHA1_HASH_SIZE   SHA1_DIGEST_SIZE
#define SHA1_MASK        (SHA1_BLOCK_SIZE - 1)

static void sha1_compile(sha1_ctx_t *ctx)
{
	uint32_t w[80], i, a, b, c, d, e, t;

	/* note that words are compiled from the buffer into 32-bit */
	/* words in big-endian order so an order reversal is needed */
	/* here on little endian machines                           */
	for (i = 0; i < SHA1_BLOCK_SIZE / 4; ++i)
		w[i] = ntohl(ctx->wbuf[i]);

	for (/*i = SHA1_BLOCK_SIZE / 4*/; i < 80; ++i) {
		t = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
		w[i] = rotl32(t, 1);
	}

	a = ctx->hash[0];
	b = ctx->hash[1];
	c = ctx->hash[2];
	d = ctx->hash[3];
	e = ctx->hash[4];

/* Reverse byte order in 32-bit words   */
#define ch(x,y,z)        ((z) ^ ((x) & ((y) ^ (z))))
#define parity(x,y,z)    ((x) ^ (y) ^ (z))
#define maj(x,y,z)       (((x) & (y)) | ((z) & ((x) | (y))))
/* A normal version as set out in the FIPS. This version uses   */
/* partial loop unrolling and is optimised for the Pentium 4    */
#define rnd(f,k) \
	do { \
		t = a; a = rotl32(a,5) + f(b,c,d) + e + k + w[i]; \
		e = d; d = c; c = rotl32(b, 30); b = t; \
	} while (0)

	for (i = 0; i < 20; ++i)
		rnd(ch, 0x5a827999);

	for (i = 20; i < 40; ++i)
		rnd(parity, 0x6ed9eba1);

	for (i = 40; i < 60; ++i)
		rnd(maj, 0x8f1bbcdc);

	for (i = 60; i < 80; ++i)
		rnd(parity, 0xca62c1d6);
#undef ch
#undef parity
#undef maj
#undef rnd

	ctx->hash[0] += a;
	ctx->hash[1] += b;
	ctx->hash[2] += c;
	ctx->hash[3] += d;
	ctx->hash[4] += e;
}

/* Process LEN bytes of BUFFER, accumulating context into CTX.
   It is assumed that LEN % 64 == 0.  */
static void sha256_process_block(const void *buffer, size_t len, sha256_ctx_t *ctx)
{
	/* Constants for SHA256 from FIPS 180-2:4.2.2.  */
	static const uint32_t K[64] = {
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
		0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
		0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
		0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
		0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
		0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
		0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
		0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
		0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	};
	const uint32_t *words = buffer;
	size_t nwords = len / sizeof(uint32_t);
	uint32_t a = ctx->H[0];
	uint32_t b = ctx->H[1];
	uint32_t c = ctx->H[2];
	uint32_t d = ctx->H[3];
	uint32_t e = ctx->H[4];
	uint32_t f = ctx->H[5];
	uint32_t g = ctx->H[6];
	uint32_t h = ctx->H[7];

	/* First increment the byte count.  FIPS 180-2 specifies the possible
	   length of the file up to 2^64 bits.  Here we only compute the
	   number of bytes.  Do a double word increment.  */
	ctx->total[0] += len;
	if (ctx->total[0] < len)
		ctx->total[1]++;

	/* Process all bytes in the buffer with 64 bytes in each round of
	   the loop.  */
	while (nwords > 0) {
		uint32_t W[64];
		uint32_t a_save = a;
		uint32_t b_save = b;
		uint32_t c_save = c;
		uint32_t d_save = d;
		uint32_t e_save = e;
		uint32_t f_save = f;
		uint32_t g_save = g;
		uint32_t h_save = h;

		/* Operators defined in FIPS 180-2:4.1.2.  */
#define Ch(x, y, z) ((x & y) ^ (~x & z))
#define Maj(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
#define S0(x) (rotr32(x, 2) ^ rotr32(x, 13) ^ rotr32(x, 22))
#define S1(x) (rotr32(x, 6) ^ rotr32(x, 11) ^ rotr32(x, 25))
#define R0(x) (rotr32(x, 7) ^ rotr32(x, 18) ^ (x >> 3))
#define R1(x) (rotr32(x, 17) ^ rotr32(x, 19) ^ (x >> 10))

		/* Compute the message schedule according to FIPS 180-2:6.2.2 step 2.  */
		for (unsigned t = 0; t < 16; ++t) {
			W[t] = ntohl(*words);
			++words;
		}
		for (unsigned t = 16; t < 64; ++t)
			W[t] = R1(W[t - 2]) + W[t - 7] + R0(W[t - 15]) + W[t - 16];

		/* The actual computation according to FIPS 180-2:6.2.2 step 3.  */
		for (unsigned t = 0; t < 64; ++t) {
			uint32_t T1 = h + S1(e) + Ch(e, f, g) + K[t] + W[t];
			uint32_t T2 = S0(a) + Maj(a, b, c);
			h = g;
			g = f;
			f = e;
			e = d + T1;
			d = c;
			c = b;
			b = a;
			a = T1 + T2;
		}
#undef Ch
#undef Maj
#undef S0
#undef S1
#undef R0
#undef R1
		/* Add the starting values of the context according to FIPS 180-2:6.2.2
		   step 4.  */
		a += a_save;
		b += b_save;
		c += c_save;
		d += d_save;
		e += e_save;
		f += f_save;
		g += g_save;
		h += h_save;

		/* Prepare for the next round.  */
		nwords -= 16;
	}

	/* Put checksum in context given as argument.  */
	ctx->H[0] = a;
	ctx->H[1] = b;
	ctx->H[2] = c;
	ctx->H[3] = d;
	ctx->H[4] = e;
	ctx->H[5] = f;
	ctx->H[6] = g;
	ctx->H[7] = h;
}

/* Process LEN bytes of BUFFER, accumulating context into CTX.
   It is assumed that LEN % 128 == 0.  */
static void sha512_process_block(const void *buffer, size_t len, sha512_ctx_t *ctx)
{
	/* Constants for SHA512 from FIPS 180-2:4.2.3.  */
	static const uint64_t K[80] = {
		0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL,
		0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
		0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
		0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
		0xd807aa98a3030242ULL, 0x12835b0145706fbeULL,
		0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
		0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL,
		0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
		0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
		0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
		0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL,
		0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
		0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL,
		0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
		0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
		0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
		0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL,
		0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
		0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL,
		0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
		0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
		0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
		0xd192e819d6ef5218ULL, 0xd69906245565a910ULL,
		0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
		0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL,
		0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
		0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
		0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
		0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL,
		0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
		0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL,
		0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
		0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
		0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
		0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL,
		0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
		0x28db77f523047d84ULL, 0x32caab7b40c72493ULL,
		0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
		0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
		0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL,
	};
	const uint64_t *words = buffer;
	size_t nwords = len / sizeof(uint64_t);
	uint64_t a = ctx->H[0];
	uint64_t b = ctx->H[1];
	uint64_t c = ctx->H[2];
	uint64_t d = ctx->H[3];
	uint64_t e = ctx->H[4];
	uint64_t f = ctx->H[5];
	uint64_t g = ctx->H[6];
	uint64_t h = ctx->H[7];

	/* First increment the byte count.  FIPS 180-2 specifies the possible
	   length of the file up to 2^128 bits.  Here we only compute the
	   number of bytes.  Do a double word increment.  */
	ctx->total[0] += len;
	if (ctx->total[0] < len)
		ctx->total[1]++;

	/* Process all bytes in the buffer with 128 bytes in each round of
	   the loop.  */
	while (nwords > 0) {
		uint64_t W[80];
		uint64_t a_save = a;
		uint64_t b_save = b;
		uint64_t c_save = c;
		uint64_t d_save = d;
		uint64_t e_save = e;
		uint64_t f_save = f;
		uint64_t g_save = g;
		uint64_t h_save = h;

		/* Operators defined in FIPS 180-2:4.1.2.  */
#define Ch(x, y, z) ((x & y) ^ (~x & z))
#define Maj(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
#define S0(x) (rotr64(x, 28) ^ rotr64(x, 34) ^ rotr64(x, 39))
#define S1(x) (rotr64(x, 14) ^ rotr64(x, 18) ^ rotr64(x, 41))
#define R0(x) (rotr64(x, 1) ^ rotr64(x, 8) ^ (x >> 7))
#define R1(x) (rotr64(x, 19) ^ rotr64(x, 61) ^ (x >> 6))

		/* Compute the message schedule according to FIPS 180-2:6.3.2 step 2.  */
		for (unsigned t = 0; t < 16; ++t) {
			W[t] = ntoh64(*words);
			++words;
		}
		for (unsigned t = 16; t < 80; ++t)
			W[t] = R1(W[t - 2]) + W[t - 7] + R0(W[t - 15]) + W[t - 16];

		/* The actual computation according to FIPS 180-2:6.3.2 step 3.  */
		for (unsigned t = 0; t < 80; ++t) {
			uint64_t T1 = h + S1(e) + Ch(e, f, g) + K[t] + W[t];
			uint64_t T2 = S0(a) + Maj(a, b, c);
			h = g;
			g = f;
			f = e;
			e = d + T1;
			d = c;
			c = b;
			b = a;
			a = T1 + T2;
		}
#undef Ch
#undef Maj
#undef S0
#undef S1
#undef R0
#undef R1
		/* Add the starting values of the context according to FIPS 180-2:6.3.2
		   step 4.  */
		a += a_save;
		b += b_save;
		c += c_save;
		d += d_save;
		e += e_save;
		f += f_save;
		g += g_save;
		h += h_save;

		/* Prepare for the next round.  */
		nwords -= 16;
	}

	/* Put checksum in context given as argument.  */
	ctx->H[0] = a;
	ctx->H[1] = b;
	ctx->H[2] = c;
	ctx->H[3] = d;
	ctx->H[4] = e;
	ctx->H[5] = f;
	ctx->H[6] = g;
	ctx->H[7] = h;
}


void FAST_FUNC sha1_begin(sha1_ctx_t *ctx)
{
	ctx->count[0] = ctx->count[1] = 0;
	ctx->hash[0] = 0x67452301;
	ctx->hash[1] = 0xefcdab89;
	ctx->hash[2] = 0x98badcfe;
	ctx->hash[3] = 0x10325476;
	ctx->hash[4] = 0xc3d2e1f0;
}

/* Initialize structure containing state of computation.
   (FIPS 180-2:5.3.2)  */
void FAST_FUNC sha256_begin(sha256_ctx_t *ctx)
{
	ctx->H[0] = 0x6a09e667;
	ctx->H[1] = 0xbb67ae85;
	ctx->H[2] = 0x3c6ef372;
	ctx->H[3] = 0xa54ff53a;
	ctx->H[4] = 0x510e527f;
	ctx->H[5] = 0x9b05688c;
	ctx->H[6] = 0x1f83d9ab;
	ctx->H[7] = 0x5be0cd19;
	ctx->total[0] = ctx->total[1] = 0;
	ctx->buflen = 0;
}

/* Initialize structure containing state of computation.
   (FIPS 180-2:5.3.3)  */
void FAST_FUNC sha512_begin(sha512_ctx_t *ctx)
{
	ctx->H[0] = 0x6a09e667f3bcc908ULL;
	ctx->H[1] = 0xbb67ae8584caa73bULL;
	ctx->H[2] = 0x3c6ef372fe94f82bULL;
	ctx->H[3] = 0xa54ff53a5f1d36f1ULL;
	ctx->H[4] = 0x510e527fade682d1ULL;
	ctx->H[5] = 0x9b05688c2b3e6c1fULL;
	ctx->H[6] = 0x1f83d9abfb41bd6bULL;
	ctx->H[7] = 0x5be0cd19137e2179ULL;
	ctx->total[0] = ctx->total[1] = 0;
	ctx->buflen = 0;
}


/* SHA1 hash data in an array of bytes into hash buffer and call the        */
/* hash_compile function as required.                                       */
void FAST_FUNC sha1_hash(const void *data, size_t length, sha1_ctx_t *ctx)
{
	uint32_t pos = (uint32_t) (ctx->count[0] & SHA1_MASK);
	uint32_t freeb = SHA1_BLOCK_SIZE - pos;
	const unsigned char *sp = data;

	ctx->count[0] += length;
	if (ctx->count[0] < length)
		ctx->count[1]++;

	while (length >= freeb) {	/* transfer whole blocks while possible  */
		memcpy(((unsigned char *) ctx->wbuf) + pos, sp, freeb);
		sp += freeb;
		length -= freeb;
		freeb = SHA1_BLOCK_SIZE;
		pos = 0;
		sha1_compile(ctx);
	}

	memcpy(((unsigned char *) ctx->wbuf) + pos, sp, length);
}

void FAST_FUNC sha256_hash(const void *buffer, size_t len, sha256_ctx_t *ctx)
{
	/* When we already have some bits in our internal buffer concatenate
	   both inputs first.  */
	if (ctx->buflen != 0) {
		size_t left_over = ctx->buflen;
		size_t add = 128 - left_over > len ? len : 128 - left_over;

		memcpy(&ctx->buffer[left_over], buffer, add);
		ctx->buflen += add;

		if (ctx->buflen > 64) {
			sha256_process_block(ctx->buffer, ctx->buflen & ~63, ctx);

			ctx->buflen &= 63;
			/* The regions in the following copy operation cannot overlap.  */
			memcpy(ctx->buffer,
			       &ctx->buffer[(left_over + add) & ~63],
			       ctx->buflen);
		}

		buffer = (const char *)buffer + add;
		len -= add;
	}

	/* Process available complete blocks.  */
	if (len >= 64) {
		if (UNALIGNED_P(buffer, uint32_t)) {
			while (len > 64) {
				sha256_process_block(memcpy(ctx->buffer, buffer, 64),
						     64, ctx);
				buffer = (const char *)buffer + 64;
				len -= 64;
			}
		} else {
			sha256_process_block(buffer, len & ~63, ctx);
			buffer = (const char *)buffer + (len & ~63);
			len &= 63;
		}
	}

	/* Move remaining bytes into internal buffer.  */
	if (len > 0) {
		size_t left_over = ctx->buflen;

		memcpy(&ctx->buffer[left_over], buffer, len);
		left_over += len;
		if (left_over >= 64) {
			sha256_process_block(ctx->buffer, 64, ctx);
			left_over -= 64;
			memcpy(ctx->buffer, &ctx->buffer[64], left_over);
		}
		ctx->buflen = left_over;
	}
}

void FAST_FUNC sha512_hash(const void *buffer, size_t len, sha512_ctx_t *ctx)
{
	/* When we already have some bits in our internal buffer concatenate
	   both inputs first.  */
	if (ctx->buflen != 0) {
		size_t left_over = ctx->buflen;
		size_t add = 256 - left_over > len ? len : 256 - left_over;

		memcpy(&ctx->buffer[left_over], buffer, add);
		ctx->buflen += add;

		if (ctx->buflen > 128) {
			sha512_process_block(ctx->buffer, ctx->buflen & ~127, ctx);

			ctx->buflen &= 127;
			/* The regions in the following copy operation cannot overlap.  */
			memcpy(ctx->buffer,
			       &ctx->buffer[(left_over + add) & ~127],
			       ctx->buflen);
		}

		buffer = (const char *)buffer + add;
		len -= add;
	}

	/* Process available complete blocks.  */
	if (len >= 128) {
// #if BB_ARCH_REQUIRES_ALIGNMENT
		if (UNALIGNED_P(buffer, uint64_t)) {
			while (len > 128) {
				sha512_process_block(memcpy(ctx->buffer, buffer, 128),
						     128, ctx);
				buffer = (const char *)buffer + 128;
				len -= 128;
			}
		} else
// #endif
		{
			sha512_process_block(buffer, len & ~127, ctx);
			buffer = (const char *)buffer + (len & ~127);
			len &= 127;
		}
	}

	/* Move remaining bytes into internal buffer.  */
	if (len > 0) {
		size_t left_over = ctx->buflen;

		memcpy(&ctx->buffer[left_over], buffer, len);
		left_over += len;
		if (left_over >= 128) {
			sha512_process_block(ctx->buffer, 128, ctx);
			left_over -= 128;
			memcpy(ctx->buffer, &ctx->buffer[128], left_over);
		}
		ctx->buflen = left_over;
	}
}


void FAST_FUNC sha1_end(void *resbuf, sha1_ctx_t *ctx)
{
	/* SHA1 Final padding and digest calculation  */
#if BB_BIG_ENDIAN
	static const uint32_t mask[4] = { 0x00000000, 0xff000000, 0xffff0000, 0xffffff00 };
	static const uint32_t bits[4] = { 0x80000000, 0x00800000, 0x00008000, 0x00000080 };
#else
	static const uint32_t mask[4] = { 0x00000000, 0x000000ff, 0x0000ffff, 0x00ffffff };
	static const uint32_t bits[4] = { 0x00000080, 0x00008000, 0x00800000, 0x80000000 };
#endif

	uint8_t *hval = resbuf;
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
	} else  /* compute a word index for the empty buffer positions */
		cnt = (cnt >> 2) + 1;

	while (cnt < 14)  /* and zero pad all but last two positions */
		ctx->wbuf[cnt++] = 0;

	/* assemble the eight byte counter in the buffer in big-endian  */
	/* format					                */
	ctx->wbuf[14] = htonl((ctx->count[1] << 3) | (ctx->count[0] >> 29));
	ctx->wbuf[15] = htonl(ctx->count[0] << 3);

	sha1_compile(ctx);

	/* extract the hash value as bytes in case the hash buffer is   */
	/* misaligned for 32-bit words                                  */
	for (i = 0; i < SHA1_DIGEST_SIZE; ++i)
		hval[i] = (unsigned char) (ctx->hash[i >> 2] >> 8 * (~i & 3));
}


/* Process the remaining bytes in the internal buffer and the usual
   prolog according to the standard and write the result to RESBUF.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 32 bits value.  */
void FAST_FUNC sha256_end(void *resbuf, sha256_ctx_t *ctx)
{
	/* Take yet unprocessed bytes into account.  */
	uint32_t bytes = ctx->buflen;
	size_t pad;

	/* Now count remaining bytes.  */
	ctx->total[0] += bytes;
	if (ctx->total[0] < bytes)
		ctx->total[1]++;

	/* Pad the buffer to the next 64-byte boundary with 0x80,0,0,0...
	   (FIPS 180-2:5.1.1)  */
	pad = (bytes >= 56 ? 64 + 56 - bytes : 56 - bytes);
	memset(&ctx->buffer[bytes], 0, pad);
	ctx->buffer[bytes] = 0x80;

	/* Put the 64-bit file length in *bits* at the end of the buffer.  */
	*(uint32_t *) &ctx->buffer[bytes + pad + 4] = ntohl(ctx->total[0] << 3);
	*(uint32_t *) &ctx->buffer[bytes + pad] = ntohl((ctx->total[1] << 3) | (ctx->total[0] >> 29));

	/* Process last bytes.  */
	sha256_process_block(ctx->buffer, bytes + pad + 8, ctx);

	/* Put result from CTX in first 32 bytes following RESBUF.  */
	for (unsigned i = 0; i < 8; ++i)
		((uint32_t *) resbuf)[i] = ntohl(ctx->H[i]);
}

/* Process the remaining bytes in the internal buffer and the usual
   prolog according to the standard and write the result to RESBUF.

   IMPORTANT: On some systems it is required that RESBUF is correctly
   aligned for a 64 bits value.  */
void FAST_FUNC sha512_end(void *resbuf, sha512_ctx_t *ctx)
{
	/* Take yet unprocessed bytes into account.  */
	uint64_t bytes = ctx->buflen;
	size_t pad;

	/* Now count remaining bytes.  */
	ctx->total[0] += bytes;
	if (ctx->total[0] < bytes)
		ctx->total[1]++;

	/* Pad the buffer to the next 128-byte boundary with 0x80,0,0,0...
	   (FIPS 180-2:5.1.2)  */
	pad = bytes >= 112 ? 128 + 112 - bytes : 112 - bytes;
	memset(&ctx->buffer[bytes], 0, pad);
	ctx->buffer[bytes] = 0x80;

	/* Put the 128-bit file length in *bits* at the end of the buffer.  */
	*(uint64_t *) &ctx->buffer[bytes + pad + 8] = hton64(ctx->total[0] << 3);
	*(uint64_t *) &ctx->buffer[bytes + pad] = hton64((ctx->total[1] << 3) | (ctx->total[0] >> 61));

	/* Process last bytes.  */
	sha512_process_block(ctx->buffer, bytes + pad + 16, ctx);

	/* Put result from CTX in first 64 bytes following RESBUF.  */
	for (unsigned i = 0; i < 8; ++i)
		((uint64_t *) resbuf)[i] = hton64(ctx->H[i]);
}
