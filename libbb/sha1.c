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

static void sha1_process_block64(sha1_ctx_t *ctx)
{
	uint32_t w[80], i, a, b, c, d, e, t;

	/* note that words are compiled from the buffer into 32-bit */
	/* words in big-endian order so an order reversal is needed */
	/* here on little endian machines                           */
	for (i = 0; i < SHA1_BLOCK_SIZE / 4; ++i)
		w[i] = ntohl(ctx->wbuffer[i]);

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

/* Constants for SHA256 from FIPS 180-2:4.2.2.  */
static const uint32_t K256[80] = {
	0x428a2f98, 0x71374491,
	0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1,
	0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01,
	0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe,
	0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786,
	0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa,
	0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d,
	0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147,
	0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138,
	0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb,
	0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b,
	0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624,
	0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08,
	0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a,
	0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f,
	0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb,
	0xbef9a3f7, 0xc67178f2,
	0xca273ece, 0xd186b8c7, /* [64]+ are used for sha512 only */
	0xeada7dd6, 0xf57d4f7f,
	0x06f067aa, 0x0a637dc5,
	0x113f9804, 0x1b710b35,
	0x28db77f5, 0x32caab7b,
	0x3c9ebe0a, 0x431d67c4,
	0x4cc5d4be, 0x597f299c,
	0x5fcb6fab, 0x6c44198c
};
/* Constants for SHA512 from FIPS 180-2:4.2.3.  */
static const uint32_t K512_lo[80] = {
	0xd728ae22, 0x23ef65cd,
	0xec4d3b2f, 0x8189dbbc,
	0xf348b538, 0xb605d019,
	0xaf194f9b, 0xda6d8118,
	0xa3030242, 0x45706fbe,
	0x4ee4b28c, 0xd5ffb4e2,
	0xf27b896f, 0x3b1696b1,
	0x25c71235, 0xcf692694,
	0x9ef14ad2, 0x384f25e3,
	0x8b8cd5b5, 0x77ac9c65,
	0x592b0275, 0x6ea6e483,
	0xbd41fbd4, 0x831153b5,
	0xee66dfab, 0x2db43210,
	0x98fb213f, 0xbeef0ee4,
	0x3da88fc2, 0x930aa725,
	0xe003826f, 0x0a0e6e70,
	0x46d22ffc, 0x5c26c926,
	0x5ac42aed, 0x9d95b3df,
	0x8baf63de, 0x3c77b2a8,
	0x47edaee6, 0x1482353b,
	0x4cf10364, 0xbc423001,
	0xd0f89791, 0x0654be30,
	0xd6ef5218, 0x5565a910,
	0x5771202a, 0x32bbd1b8,
	0xb8d2d0c8, 0x5141ab53,
	0xdf8eeb99, 0xe19b48a8,
	0xc5c95a63, 0xe3418acb,
	0x7763e373, 0xd6b2b8a3,
	0x5defb2fc, 0x43172f60,
	0xa1f0ab72, 0x1a6439ec,
	0x23631e28, 0xde82bde9,
	0xb2c67915, 0xe372532b,
	0xea26619c, 0x21c0c207,
	0xcde0eb1e, 0xee6ed178,
	0x72176fba, 0xa2c898a6,
	0xbef90dae, 0x131c471b,
	0x23047d84, 0x40c72493,
	0x15c9bebc, 0x9c100d4c,
	0xcb3e42b6, 0xfc657e2a,
	0x3ad6faec, 0x4a475817
};

/* Process LEN bytes of BUFFER, accumulating context into CTX.
   LEN is rounded _down_ to 64.  */
static void sha256_process_block64(const void *buffer, size_t len, sha256_ctx_t *ctx)
{
	const uint32_t *words = buffer;
	uint32_t a = ctx->hash[0];
	uint32_t b = ctx->hash[1];
	uint32_t c = ctx->hash[2];
	uint32_t d = ctx->hash[3];
	uint32_t e = ctx->hash[4];
	uint32_t f = ctx->hash[5];
	uint32_t g = ctx->hash[6];
	uint32_t h = ctx->hash[7];

	/* Process all bytes in the buffer with 64 bytes in each round of
	   the loop.  */
	len /= (sizeof(uint32_t) * 16);
	while (len) {
		unsigned t;
		uint32_t W[64];

		/* Operators defined in FIPS 180-2:4.1.2.  */
#define Ch(x, y, z) ((x & y) ^ (~x & z))
#define Maj(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
#define S0(x) (rotr32(x, 2) ^ rotr32(x, 13) ^ rotr32(x, 22))
#define S1(x) (rotr32(x, 6) ^ rotr32(x, 11) ^ rotr32(x, 25))
#define R0(x) (rotr32(x, 7) ^ rotr32(x, 18) ^ (x >> 3))
#define R1(x) (rotr32(x, 17) ^ rotr32(x, 19) ^ (x >> 10))

		/* Compute the message schedule according to FIPS 180-2:6.2.2 step 2.  */
		for (t = 0; t < 16; ++t) {
			W[t] = ntohl(*words);
			++words;
		}

		for (/*t = 16*/; t < 64; ++t)
			W[t] = R1(W[t - 2]) + W[t - 7] + R0(W[t - 15]) + W[t - 16];

		/* The actual computation according to FIPS 180-2:6.2.2 step 3.  */
		for (t = 0; t < 64; ++t) {
			uint32_t T1 = h + S1(e) + Ch(e, f, g) + K256[t] + W[t];
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
		ctx->hash[0] = a += ctx->hash[0];
		ctx->hash[1] = b += ctx->hash[1];
		ctx->hash[2] = c += ctx->hash[2];
		ctx->hash[3] = d += ctx->hash[3];
		ctx->hash[4] = e += ctx->hash[4];
		ctx->hash[5] = f += ctx->hash[5];
		ctx->hash[6] = g += ctx->hash[6];
		ctx->hash[7] = h += ctx->hash[7];

		/* Prepare for the next round.  */
		len--;
	}
}
/* Process LEN bytes of BUFFER, accumulating context into CTX.
   LEN is rounded _down_ to 128.  */
static void sha512_process_block128(const void *buffer, size_t len, sha512_ctx_t *ctx)
{
	const uint64_t *words = buffer;
	uint64_t a = ctx->hash[0];
	uint64_t b = ctx->hash[1];
	uint64_t c = ctx->hash[2];
	uint64_t d = ctx->hash[3];
	uint64_t e = ctx->hash[4];
	uint64_t f = ctx->hash[5];
	uint64_t g = ctx->hash[6];
	uint64_t h = ctx->hash[7];

	len /= (sizeof(uint64_t) * 16);
	while (len) {
		unsigned t;
		uint64_t W[80];

		/* Operators defined in FIPS 180-2:4.1.2.  */
#define Ch(x, y, z) ((x & y) ^ (~x & z))
#define Maj(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
#define S0(x) (rotr64(x, 28) ^ rotr64(x, 34) ^ rotr64(x, 39))
#define S1(x) (rotr64(x, 14) ^ rotr64(x, 18) ^ rotr64(x, 41))
#define R0(x) (rotr64(x, 1) ^ rotr64(x, 8) ^ (x >> 7))
#define R1(x) (rotr64(x, 19) ^ rotr64(x, 61) ^ (x >> 6))

		/* Compute the message schedule according to FIPS 180-2:6.3.2 step 2.  */
		for (t = 0; t < 16; ++t) {
			W[t] = ntoh64(*words);
			++words;
		}
		for (/*t = 16*/; t < 80; ++t)
			W[t] = R1(W[t - 2]) + W[t - 7] + R0(W[t - 15]) + W[t - 16];

		/* The actual computation according to FIPS 180-2:6.3.2 step 3.  */
		for (t = 0; t < 80; ++t) {
			uint64_t K512_t = ((uint64_t)(K256[t]) << 32) + K512_lo[t];
			uint64_t T1 = h + S1(e) + Ch(e, f, g) + K512_t + W[t];
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
		ctx->hash[0] = a += ctx->hash[0];
		ctx->hash[1] = b += ctx->hash[1];
		ctx->hash[2] = c += ctx->hash[2];
		ctx->hash[3] = d += ctx->hash[3];
		ctx->hash[4] = e += ctx->hash[4];
		ctx->hash[5] = f += ctx->hash[5];
		ctx->hash[6] = g += ctx->hash[6];
		ctx->hash[7] = h += ctx->hash[7];

		len--;
	}
}


void FAST_FUNC sha1_begin(sha1_ctx_t *ctx)
{
	ctx->total64 = 0;
	ctx->hash[0] = 0x67452301;
	ctx->hash[1] = 0xefcdab89;
	ctx->hash[2] = 0x98badcfe;
	ctx->hash[3] = 0x10325476;
	ctx->hash[4] = 0xc3d2e1f0;
}

static const uint32_t init256[] = {
	0x6a09e667,
	0xbb67ae85,
	0x3c6ef372,
	0xa54ff53a,
	0x510e527f,
	0x9b05688c,
	0x1f83d9ab,
	0x5be0cd19
};
static const uint32_t init512_lo[] = {
	0xf3bcc908,
	0x84caa73b,
	0xfe94f82b,
	0x5f1d36f1,
	0xade682d1,
	0x2b3e6c1f,
	0xfb41bd6b,
	0x137e2179
};
/* Initialize structure containing state of computation.
   (FIPS 180-2:5.3.2)  */
void FAST_FUNC sha256_begin(sha256_ctx_t *ctx)
{
	memcpy(ctx->hash, init256, sizeof(init256));
	ctx->total64 = 0;
}
/* Initialize structure containing state of computation.
   (FIPS 180-2:5.3.3)  */
void FAST_FUNC sha512_begin(sha512_ctx_t *ctx)
{
	int i;
	for (i = 0; i < 8; i++)
		ctx->hash[i] = ((uint64_t)(init256[i]) << 32) + init512_lo[i];
	ctx->total64[0] = ctx->total64[1] = 0;
}


/* SHA1 hash data in an array of bytes into hash buffer and call the        */
/* hash_compile function as required.                                       */
void FAST_FUNC sha1_hash(const void *buffer, size_t len, sha1_ctx_t *ctx)
{
	unsigned in_buf = ctx->total64 & SHA1_MASK;
	unsigned add = SHA1_BLOCK_SIZE - in_buf;

	ctx->total64 += len;

	while (len >= add) {	/* transfer whole blocks while possible  */
		memcpy(((unsigned char *) ctx->wbuffer) + in_buf, buffer, add);
		buffer = (const char *)buffer + add;
		len -= add;
		add = SHA1_BLOCK_SIZE;
		in_buf = 0;
		sha1_process_block64(ctx);
	}

	memcpy(((unsigned char *) ctx->wbuffer) + in_buf, buffer, len);
}

void FAST_FUNC sha256_hash(const void *buffer, size_t len, sha256_ctx_t *ctx)
{
	unsigned in_buf = ctx->total64 & 63;

	/* First increment the byte count.  FIPS 180-2 specifies the possible
	   length of the file up to 2^64 _bits_.
	   We compute the number of _bytes_ and convert to bits later.  */
	ctx->total64 += len;

	/* When we already have some bits in our internal buffer concatenate
	   both inputs first.  */
	if (in_buf != 0) {
		unsigned add;

		/* NB: 1/2 of wbuffer is used only in sha256_end
		 * when length field is added and hashed.
		 * With buffer twice as small, it may happen that
		 * we have it almost full and can't add length field.  */

		add = sizeof(ctx->wbuffer)/2 - in_buf;
		if (add > len)
			add = len;
		memcpy(&ctx->wbuffer[in_buf], buffer, add);
		in_buf += add;

		/* If we still didn't collect full wbuffer, bail out */
		if (in_buf < sizeof(ctx->wbuffer)/2)
			return;

		sha256_process_block64(ctx->wbuffer, 64, ctx);
		buffer = (const char *)buffer + add;
		len -= add;
	}

	/* Process available complete blocks.  */
	if (len >= 64) {
		if (UNALIGNED_P(buffer, uint32_t)) {
			while (len >= 64) {
				sha256_process_block64(memcpy(ctx->wbuffer, buffer, 64), 64, ctx);
				buffer = (const char *)buffer + 64;
				len -= 64;
			}
		} else {
			sha256_process_block64(buffer, len /*& ~63*/, ctx);
			buffer = (const char *)buffer + (len & ~63);
			len &= 63;
		}
	}

	/* Move remaining bytes into internal buffer.  */
	if (len > 0) {
		memcpy(ctx->wbuffer, buffer, len);
	}
}

void FAST_FUNC sha512_hash(const void *buffer, size_t len, sha512_ctx_t *ctx)
{
	unsigned in_buf = ctx->total64[0] & 127;

	/* First increment the byte count.  FIPS 180-2 specifies the possible
	   length of the file up to 2^128 _bits_.
	   We compute the number of _bytes_ and convert to bits later.  */
	ctx->total64[0] += len;
	if (ctx->total64[0] < len)
		ctx->total64[1]++;

	if (in_buf != 0) {
		unsigned add;

		add = sizeof(ctx->wbuffer)/2 - in_buf;
		if (add > len)
			add = len;
		memcpy(&ctx->wbuffer[in_buf], buffer, add);
		in_buf += add;

		if (in_buf < sizeof(ctx->wbuffer)/2)
			return;

		sha512_process_block128(ctx->wbuffer, 128, ctx);
		buffer = (const char *)buffer + add;
		len -= add;
	}

	if (len >= 128) {
		if (UNALIGNED_P(buffer, uint64_t)) {
			while (len >= 128) {
				sha512_process_block128(memcpy(ctx->wbuffer, buffer, 128), 128, ctx);
				buffer = (const char *)buffer + 128;
				len -= 128;
			}
		} else {
			sha512_process_block128(buffer, len /*& ~127*/, ctx);
			buffer = (const char *)buffer + (len & ~127);
			len &= 127;
		}
	}

	if (len > 0) {
		memcpy(ctx->wbuffer, buffer, len);
	}
}


void FAST_FUNC sha1_end(void *resbuf, sha1_ctx_t *ctx)
{
	unsigned i, pad, in_buf;

	/* Pad the buffer to the next 64-byte boundary with 0x80,0,0,0... */
	in_buf = ctx->total64 & SHA1_MASK;
	((uint8_t *)ctx->wbuffer)[in_buf++] = 0x80;
	pad = SHA1_BLOCK_SIZE - in_buf;
	memset(((uint8_t *)ctx->wbuffer) + in_buf, 0, pad);

	/* We need 1+8 or more empty positions, one for the padding byte
	 * (above) and eight for the length count.
	 * If there is not enough space, empty the buffer. */
	if (pad < 8) {
		sha1_process_block64(ctx);
		memset(ctx->wbuffer, 0, SHA1_BLOCK_SIZE - 8);
		((uint8_t *)ctx->wbuffer)[0] = 0x80;
	}

	/* Store the 64-bit counter of bits in the buffer in BE format */
	{
		uint64_t t = ctx->total64 << 3;
		t = hton64(t);
		/* wbuffer is suitably aligned for this */
		*(uint64_t *) &ctx->wbuffer[14] = t;
	}

	sha1_process_block64(ctx);

#if BB_LITTLE_ENDIAN
	for (i = 0; i < ARRAY_SIZE(ctx->hash); ++i)
		ctx->hash[i] = htonl(ctx->hash[i]);
#endif
	memcpy(resbuf, ctx->hash, sizeof(ctx->hash));
}

void FAST_FUNC sha256_end(void *resbuf, sha256_ctx_t *ctx)
{
	unsigned i, pad, in_buf;

	/* Pad the buffer to the next 64-byte boundary with 0x80,0,0,0...
	   (FIPS 180-2:5.1.1)  */
	in_buf = ctx->total64 & 63;
	pad = (in_buf >= 56 ? 64 + 56 - in_buf : 56 - in_buf);
	memset(&ctx->wbuffer[in_buf], 0, pad);
	ctx->wbuffer[in_buf] = 0x80;

	/* Put the 64-bit file length in *bits* at the end of the buffer.  */
	{
		uint64_t t = ctx->total64 << 3;
		t = hton64(t);
		/* wbuffer is suitably aligned for this */
		*(uint64_t *) &ctx->wbuffer[in_buf + pad] = t;
	}

	/* Process last bytes.  */
	sha256_process_block64(ctx->wbuffer, in_buf + pad + 8, ctx);

#if BB_LITTLE_ENDIAN
	for (i = 0; i < ARRAY_SIZE(ctx->hash); ++i)
		ctx->hash[i] = htonl(ctx->hash[i]);
#endif
	memcpy(resbuf, ctx->hash, sizeof(ctx->hash));
}

void FAST_FUNC sha512_end(void *resbuf, sha512_ctx_t *ctx)
{
	unsigned i, pad, in_buf;

	/* Pad the buffer to the next 128-byte boundary with 0x80,0,0,0...
	   (FIPS 180-2:5.1.2)  */
	in_buf = ctx->total64[0] & 127;
	pad = in_buf >= 112 ? 128 + 112 - in_buf : 112 - in_buf;
	memset(&ctx->wbuffer[in_buf], 0, pad);
	ctx->wbuffer[in_buf] = 0x80;

	*(uint64_t *) &ctx->wbuffer[in_buf + pad + 8] = hton64(ctx->total64[0] << 3);
	*(uint64_t *) &ctx->wbuffer[in_buf + pad] = hton64((ctx->total64[1] << 3) | (ctx->total64[0] >> 61));

	sha512_process_block128(ctx->wbuffer, in_buf + pad + 16, ctx);

#if BB_LITTLE_ENDIAN
	for (i = 0; i < ARRAY_SIZE(ctx->hash); ++i)
		ctx->hash[i] = hton64(ctx->hash[i]);
#endif
	memcpy(resbuf, ctx->hash, sizeof(ctx->hash));
}
