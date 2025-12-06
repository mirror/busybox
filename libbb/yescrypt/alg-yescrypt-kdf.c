/*-
 * Copyright 2009 Colin Percival
 * Copyright 2012-2018 Alexander Peslyak
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * This file was originally written by Colin Percival as part of the Tarsnap
 * online backup system.
 */

#if __STDC_VERSION__ >= 199901L
/* Have restrict */
#elif defined(__GNUC__)
#define restrict __restrict
#else
#define restrict
#endif

#ifdef __GNUC__
#define unlikely(exp) __builtin_expect(exp, 0)
#else
#define unlikely(exp) (exp)
#endif

typedef union {
	uint32_t w[16];
	uint64_t d[8];
} salsa20_blk_t;

static void salsa20_simd_shuffle(
		const salsa20_blk_t *Bin,
		salsa20_blk_t *Bout)
{
#define COMBINE(out, in1, in2) \
do { \
	Bout->d[out] = Bin->w[in1 * 2] | ((uint64_t)Bin->w[in2 * 2 + 1] << 32); \
} while (0)
	COMBINE(0, 0, 2);
	COMBINE(1, 5, 7);
	COMBINE(2, 2, 4);
	COMBINE(3, 7, 1);
	COMBINE(4, 4, 6);
	COMBINE(5, 1, 3);
	COMBINE(6, 6, 0);
	COMBINE(7, 3, 5);
#undef COMBINE
}

static void salsa20_simd_unshuffle(
		const salsa20_blk_t *Bin,
		salsa20_blk_t *Bout)
{
#define UNCOMBINE(out, in1, in2) \
do { \
	Bout->w[out * 2] = Bin->d[in1]; \
	Bout->w[out * 2 + 1] = Bin->d[in2] >> 32; \
} while (0)
	UNCOMBINE(0, 0, 6);
	UNCOMBINE(1, 5, 3);
	UNCOMBINE(2, 2, 0);
	UNCOMBINE(3, 7, 5);
	UNCOMBINE(4, 4, 2);
	UNCOMBINE(5, 1, 7);
	UNCOMBINE(6, 6, 4);
	UNCOMBINE(7, 3, 1);
#undef UNCOMBINE
}

#define DECL_X \
	salsa20_blk_t X
#define DECL_Y \
	salsa20_blk_t Y

#if KDF_UNROLL_COPY
#define COPY(out, in) \
do { \
	(out).d[0] = (in).d[0]; \
	(out).d[1] = (in).d[1]; \
	(out).d[2] = (in).d[2]; \
	(out).d[3] = (in).d[3]; \
	(out).d[4] = (in).d[4]; \
	(out).d[5] = (in).d[5]; \
	(out).d[6] = (in).d[6]; \
	(out).d[7] = (in).d[7]; \
} while (0)
#else
#define COPY(out, in) \
do { \
	memcpy((out).d, (in).d, sizeof((in).d)); \
} while (0)
#endif

#define READ_X(in)   COPY(X, in)
#define WRITE_X(out) COPY(out, X)

/**
 * salsa20(B):
 * Apply the Salsa20 core to the provided block.
 */
static void salsa20(salsa20_blk_t *restrict B,
		salsa20_blk_t *restrict Bout,
		uint32_t doublerounds)
{
	salsa20_blk_t X;
#define x X.w

	salsa20_simd_unshuffle(B, &X);

	do {
#define R(a,b) (((a) << (b)) | ((a) >> (32 - (b))))
		/* Operate on columns */
#if KDF_UNROLL_SALSA20
		x[ 4] ^= R(x[ 0]+x[12], 7); // x[j] ^= R(x[k]+x[l], CONST)
		x[ 8] ^= R(x[ 4]+x[ 0], 9);
		x[12] ^= R(x[ 8]+x[ 4],13);
		x[ 0] ^= R(x[12]+x[ 8],18);

		x[ 9] ^= R(x[ 5]+x[ 1], 7);
		x[13] ^= R(x[ 9]+x[ 5], 9);
		x[ 1] ^= R(x[13]+x[ 9],13);
		x[ 5] ^= R(x[ 1]+x[13],18);

		x[14] ^= R(x[10]+x[ 6], 7);
		x[ 2] ^= R(x[14]+x[10], 9);
		x[ 6] ^= R(x[ 2]+x[14],13);
		x[10] ^= R(x[ 6]+x[ 2],18);

		x[ 3] ^= R(x[15]+x[11], 7);
		x[ 7] ^= R(x[ 3]+x[15], 9);
		x[11] ^= R(x[ 7]+x[ 3],13);
		x[15] ^= R(x[11]+x[ 7],18);
#else
		{
			unsigned j, k, l;
			j = 4; k = 0; l = 12;
			for (;;) {
				uint32_t t;
				x[j] ^= ({ t = x[k] + x[l]; R(t, 7); }); l = k; k = j; j = (j+4) & 0xf;
				x[j] ^= ({ t = x[k] + x[l]; R(t, 9); }); l = k; k = j; j = (j+4) & 0xf;
				x[j] ^= ({ t = x[k] + x[l]; R(t,13); }); l = k; k = j; j = (j+4) & 0xf;
				x[j] ^= ({ t = x[k] + x[l]; R(t,18); });
				if (j == 15) break;
				l = j + 1; k = j + 5; j = (j+9) & 0xf;
			}
		}
#endif
		/* Operate on rows */
#if KDF_UNROLL_SALSA20
// i=0 n=0
		x[ 1] ^= R(x[ 0]+x[ 3], 7); // [i + (n+1)&3] [i + (n+0)&3] [i + (n+3)&3]
		x[ 2] ^= R(x[ 1]+x[ 0], 9); // [i + (n+2)&3] [i + (n+1)&3] [i + (n+0)&3]
		x[ 3] ^= R(x[ 2]+x[ 1],13); // [i + (n+3)&3] [i + (n+2)&3] [i + (n+1)&3]
		x[ 0] ^= R(x[ 3]+x[ 2],18); // [i + (n+0)&3] [i + (n+3)&3] [i + (n+2)&3]
// i=4 n=1                                          ^^^j^^^       ^^^k^^^       ^^^l^^^
		x[ 6] ^= R(x[ 5]+x[ 4], 7); // [i + (n+1)&3] [i + (n+0)&3] [i + (n+3)&3]
		x[ 7] ^= R(x[ 6]+x[ 5], 9); // [i + (n+2)&3] [i + (n+1)&3] [i + (n+0)&3]
		x[ 4] ^= R(x[ 7]+x[ 6],13); // [i + (n+3)&3] [i + (n+2)&3] [i + (n+1)&3]
		x[ 5] ^= R(x[ 4]+x[ 7],18); // [i + (n+0)&3] [i + (n+3)&3] [i + (n+2)&3]
// i=8 n=2
		x[11] ^= R(x[10]+x[ 9], 7); // [i + (n+1)&3] [i + (n+0)&3] [i + (n+3)&3]
		x[ 8] ^= R(x[11]+x[10], 9); // [i + (n+2)&3] [i + (n+1)&3] [i + (n+0)&3]
		x[ 9] ^= R(x[ 8]+x[11],13); // [i + (n+3)&3] [i + (n+2)&3] [i + (n+1)&3]
		x[10] ^= R(x[ 9]+x[ 8],18); // [i + (n+0)&3] [i + (n+3)&3] [i + (n+2)&3]
// i=12 n=3
		x[12] ^= R(x[15]+x[14], 7); // [i + (n+1)&3] [i + (n+0)&3] [i + (n+3)&3]
		x[13] ^= R(x[12]+x[15], 9); // [i + (n+2)&3] [i + (n+1)&3] [i + (n+0)&3]
		x[14] ^= R(x[13]+x[12],13); // [i + (n+3)&3] [i + (n+2)&3] [i + (n+1)&3]
		x[15] ^= R(x[14]+x[13],18); // [i + (n+0)&3] [i + (n+3)&3] [i + (n+2)&3]
#else
		{
			unsigned j, k, l;
			uint32_t *xrow;
			j = 1; k = 0; l = 3;
			xrow = &x[0];
			for (;;) {
				uint32_t t;
				xrow[j] ^= ({ t = xrow[k] + xrow[l]; R(t, 7); }); l = k; k = j; j = (j+1) & 3;
				xrow[j] ^= ({ t = xrow[k] + xrow[l]; R(t, 9); }); l = k; k = j; j = (j+1) & 3;
				xrow[j] ^= ({ t = xrow[k] + xrow[l]; R(t,13); }); l = k; k = j; j = (j+1) & 3;
				xrow[j] ^= ({ t = xrow[k] + xrow[l]; R(t,18); });
				if (j == 3) break;
				l = j; k = j + 1; j = (j+2) & 3;
				xrow += 4;
			}
		}
#endif

#undef R
	} while (--doublerounds);
#undef x

	{
		uint32_t i;
		salsa20_simd_shuffle(&X, Bout);
		for (i = 0; i < 16; i++) {
			// bbox: note: was unrolled x4
			B->w[i] = Bout->w[i] += B->w[i];
		}
	}
#if 0
	/* Too expensive */
	explicit_bzero(&X, sizeof(X));
#endif
}

/**
 * Apply the Salsa20/2 core to the block provided in X.
 */
#define SALSA20_2(out) \
	salsa20(&X, &out, 1)

#if 0
#define XOR(out, in1, in2) \
do { \
	(out).d[0] = (in1).d[0] ^ (in2).d[0]; \
	(out).d[1] = (in1).d[1] ^ (in2).d[1]; \
	(out).d[2] = (in1).d[2] ^ (in2).d[2]; \
	(out).d[3] = (in1).d[3] ^ (in2).d[3]; \
	(out).d[4] = (in1).d[4] ^ (in2).d[4]; \
	(out).d[5] = (in1).d[5] ^ (in2).d[5]; \
	(out).d[6] = (in1).d[6] ^ (in2).d[6]; \
	(out).d[7] = (in1).d[7] ^ (in2).d[7]; \
} while (0)
#else
#define XOR(out, in1, in2) \
do { \
	xorbuf64_3_aligned64(&(out).d, &(in1).d, &(in2).d); \
} while (0)
#endif

#define XOR_X(in)         XOR(X, X, in)
#define XOR_X_2(in1, in2) XOR(X, in1, in2)
#define XOR_X_WRITE_XOR_Y_2(out, in) \
do { \
	XOR(Y, out, in); \
	COPY(out, Y); \
	XOR(X, X, Y); \
} while (0)

/**
 * Apply the Salsa20/8 core to the block provided in X ^ in.
 */
#define SALSA20_8_XOR_MEM(in, out) \
do { \
	XOR_X(in); \
	salsa20(&X, &out, 4); \
} while (0)

#define INTEGERIFY ((uint32_t)X.d[0])

/**
 * blockmix_salsa8(Bin, Bout, r):
 * Compute Bout = BlockMix_{salsa20/8, r}(Bin).  The input Bin must be 128r
 * bytes in length; the output Bout must also be the same size.
 */
static void blockmix_salsa8(
		const salsa20_blk_t *restrict Bin,
		salsa20_blk_t *restrict Bout,
		size_t r)
{
	size_t i;
	DECL_X;

	READ_X(Bin[r * 2 - 1]);
	for (i = 0; i < r; i++) {
		SALSA20_8_XOR_MEM(Bin[i * 2], Bout[i]);
		SALSA20_8_XOR_MEM(Bin[i * 2 + 1], Bout[r + i]);
	}
}

static uint32_t blockmix_salsa8_xor(
		const salsa20_blk_t *restrict Bin1,
		const salsa20_blk_t *restrict Bin2,
		salsa20_blk_t *restrict Bout,
		size_t r)
{
	size_t i;
	DECL_X;

	XOR_X_2(Bin1[r * 2 - 1], Bin2[r * 2 - 1]);
	for (i = 0; i < r; i++) {
		XOR_X(Bin1[i * 2]);
		SALSA20_8_XOR_MEM(Bin2[i * 2], Bout[i]);
		XOR_X(Bin1[i * 2 + 1]);
		SALSA20_8_XOR_MEM(Bin2[i * 2 + 1], Bout[r + i]);
	}

	return INTEGERIFY;
}

/* This is tunable */
#define Swidth 8

/* Not tunable in this implementation, hard-coded in a few places */
#define PWXsimple 2
#define PWXgather 4

/* Derived values.  Not tunable except via Swidth above. */
#define PWXbytes (PWXgather * PWXsimple * 8)
#define Sbytes   (3 * (1 << Swidth) * PWXsimple * 8)
#define Smask    (((1 << Swidth) - 1) * PWXsimple * 8)
#define Smask2   (((uint64_t)Smask << 32) | Smask)

#define DECL_SMASK2REG       do {} while (0)
#define FORCE_REGALLOC_3     do {} while (0)
#define MAYBE_MEMORY_BARRIER do {} while (0)

#define PWXFORM_SIMD(x0, x1) \
do { \
	uint64_t x = x0 & Smask2; \
	uint64_t *p0 = (uint64_t *)(S0 + (uint32_t)x); \
	uint64_t *p1 = (uint64_t *)(S1 + (x >> 32)); \
	x0 = ((x0 >> 32) * (uint32_t)x0 + p0[0]) ^ p1[0]; \
	x1 = ((x1 >> 32) * (uint32_t)x1 + p0[1]) ^ p1[1]; \
} while (0)

#if KDF_UNROLL_PWXFORM_ROUND
#define PWXFORM_ROUND \
do { \
	PWXFORM_SIMD(X.d[0], X.d[1]); \
	PWXFORM_SIMD(X.d[2], X.d[3]); \
	PWXFORM_SIMD(X.d[4], X.d[5]); \
	PWXFORM_SIMD(X.d[6], X.d[7]); \
} while (0)
#else
#define PWXFORM_ROUND \
do { \
	for (int pwxi=0; pwxi<8; pwxi+=2) \
		PWXFORM_SIMD(X.d[pwxi], X.d[pwxi + 1]); \
} while (0)
#endif

/*
 * This offset helps address the 256-byte write block via the single-byte
 * displacements encodable in x86(-64) instructions.  It is needed because the
 * displacements are signed.  Without it, we'd get 4-byte displacements for
 * half of the writes.  Setting it to 0x80 instead of 0x7c would avoid needing
 * a displacement for one of the writes, but then the LEA instruction would
 * need a 4-byte displacement.
 */
#define PWXFORM_WRITE_OFFSET 0x7c

#define PWXFORM_WRITE \
do { \
	WRITE_X(*(salsa20_blk_t *)(Sw - PWXFORM_WRITE_OFFSET)); \
	Sw += 64; \
} while (0)

#if KDF_UNROLL_PWXFORM
#define PWXFORM \
do { \
	uint8_t *Sw = S2 + w + PWXFORM_WRITE_OFFSET; \
	FORCE_REGALLOC_3; \
	MAYBE_MEMORY_BARRIER; \
	PWXFORM_ROUND; \
	PWXFORM_ROUND; PWXFORM_WRITE; \
	PWXFORM_ROUND; PWXFORM_WRITE; \
	PWXFORM_ROUND; PWXFORM_WRITE; \
	PWXFORM_ROUND; PWXFORM_WRITE; \
	PWXFORM_ROUND; \
	w = (w + 64 * 4) & Smask2; \
	{ \
		uint8_t *Stmp = S2; \
		S2 = S1; \
		S1 = S0; \
		S0 = Stmp; \
	} \
} while (0)
#else
#define PWXFORM \
do { \
	uint8_t *Sw = S2 + w + PWXFORM_WRITE_OFFSET; \
	FORCE_REGALLOC_3; \
	MAYBE_MEMORY_BARRIER; \
	PWXFORM_ROUND; \
	for (int pwxj=0; pwxj<4; pwxj++) {\
		PWXFORM_ROUND; PWXFORM_WRITE; \
	} \
	PWXFORM_ROUND; \
	w = (w + 64 * 4) & Smask2; \
	{ \
		uint8_t *Stmp = S2; \
		S2 = S1; \
		S1 = S0; \
		S0 = Stmp; \
	} \
} while (0)
#endif

typedef struct {
	uint8_t *S0, *S1, *S2;
	size_t w;
} pwxform_ctx_t;

#define Salloc (Sbytes + ((sizeof(pwxform_ctx_t) + 63) & ~63U))

/**
 * blockmix_pwxform(Bin, Bout, r, S):
 * Compute Bout = BlockMix_pwxform{salsa20/2, r, S}(Bin).  The input Bin must
 * be 128r bytes in length; the output Bout must also be the same size.
 */
static void blockmix(
		const salsa20_blk_t *restrict Bin,
		salsa20_blk_t *restrict Bout,
		size_t r,
		pwxform_ctx_t *restrict ctx)
{
	uint8_t *S0 = ctx->S0, *S1 = ctx->S1, *S2 = ctx->S2;
	size_t w = ctx->w;
	size_t i;
	DECL_X;

	/* Convert count of 128-byte blocks to max index of 64-byte block */
	r = r * 2 - 1;

	READ_X(Bin[r]);

	DECL_SMASK2REG;

	i = 0;
	for (;;) {
		XOR_X(Bin[i]);
		PWXFORM;
		if (unlikely(i >= r))
			break;
		WRITE_X(Bout[i]);
		i++;
	}

	ctx->S0 = S0;
	ctx->S1 = S1;
	ctx->S2 = S2;
	ctx->w = w;

	SALSA20_2(Bout[i]);
}

static uint32_t blockmix_xor(
		const salsa20_blk_t *Bin1,
		const salsa20_blk_t *restrict Bin2,
		salsa20_blk_t *Bout,
		size_t r,
		pwxform_ctx_t *restrict ctx)
{
	uint8_t *S0 = ctx->S0, *S1 = ctx->S1, *S2 = ctx->S2;
	size_t w = ctx->w;
	size_t i;
	DECL_X;

	/* Convert count of 128-byte blocks to max index of 64-byte block */
	r = r * 2 - 1;

	XOR_X_2(Bin1[r], Bin2[r]);

	DECL_SMASK2REG;

	i = 0;
	r--;
	for (;;) {
		XOR_X(Bin1[i]);
		XOR_X(Bin2[i]);
		PWXFORM;
		if (unlikely(i > r))
			break;
		WRITE_X(Bout[i]);
		i++;
	}

	ctx->S0 = S0;
	ctx->S1 = S1;
	ctx->S2 = S2;
	ctx->w = w;

	SALSA20_2(Bout[i]);

	return INTEGERIFY;
}

static uint32_t blockmix_xor_save(
		salsa20_blk_t *restrict Bin1out,
		salsa20_blk_t *restrict Bin2,
		size_t r,
		pwxform_ctx_t *restrict ctx)
{
	uint8_t *S0 = ctx->S0, *S1 = ctx->S1, *S2 = ctx->S2;
	size_t w = ctx->w;
	size_t i;
	DECL_X;
	DECL_Y;

	/* Convert count of 128-byte blocks to max index of 64-byte block */
	r = r * 2 - 1;

	XOR_X_2(Bin1out[r], Bin2[r]);

	DECL_SMASK2REG;

	i = 0;
	r--;
	for (;;) {
		XOR_X_WRITE_XOR_Y_2(Bin2[i], Bin1out[i]);
		PWXFORM;
		if (unlikely(i > r))
			break;
		WRITE_X(Bin1out[i]);
		i++;
	}

	ctx->S0 = S0;
	ctx->S1 = S1;
	ctx->S2 = S2;
	ctx->w = w;

	SALSA20_2(Bin1out[i]);

	return INTEGERIFY;
}

/**
 * integerify(B, r):
 * Return the result of parsing B_{2r-1} as a little-endian integer.
 */
static inline uint32_t integerify(const salsa20_blk_t *B, size_t r)
{
/*
 * Our 64-bit words are in host byte order, which is why we don't just read
 * w[0] here (would be wrong on big-endian).  Also, our 32-bit words are
 * SIMD-shuffled (so the next 32 bits would be part of d[6]), but currently
 * this does not matter as we only care about the least significant 32 bits.
 */
	return (uint32_t)B[2 * r - 1].d[0];
}

/**
 * smix1(B, r, N, flags, V, NROM, VROM, XY, ctx):
 * Compute first loop of B = SMix_r(B, N).  The input B must be 128r bytes in
 * length; the temporary storage V must be 128rN bytes in length; the temporary
 * storage XY must be 128r+64 bytes in length.  N must be even and at least 4.
 * The array V must be aligned to a multiple of 64 bytes, and arrays B and XY
 * to a multiple of at least 16 bytes.
 */
#if DISABLE_NROM_CODE
#define smix1(B,r,N,flags,V,NROM,VROM,XY,ctx) \
	smix1(B,r,N,flags,V,XY,ctx)
#endif
static void smix1(uint8_t *B, size_t r, uint32_t N,
		uint32_t flags,
		salsa20_blk_t *V,
		uint32_t NROM, const salsa20_blk_t *VROM,
		salsa20_blk_t *XY,
		pwxform_ctx_t *ctx)
{
#if DISABLE_NROM_CODE
	uint32_t NROM = 0;
	const salsa20_blk_t *VROM = NULL;
#endif
	size_t s = 2 * r;
	salsa20_blk_t *X = V, *Y = &V[s];
	uint32_t i, j;

	for (i = 0; i < 2 * r; i++) {
		const salsa20_blk_t *src = (salsa20_blk_t *)&B[i * 64];
		salsa20_blk_t *tmp = Y;
		salsa20_blk_t *dst = &X[i];
		size_t k;
		for (k = 0; k < 16; k++)
			tmp->w[k] = SWAP_LE32(src->w[k]);
		salsa20_simd_shuffle(tmp, dst);
	}

	if (VROM) {
		uint32_t n;
		const salsa20_blk_t *V_j;

		V_j = &VROM[(NROM - 1) * s];
		j = blockmix_xor(X, V_j, Y, r, ctx) & (NROM - 1);
		V_j = &VROM[j * s];
		X = Y + s;
		j = blockmix_xor(Y, V_j, X, r, ctx);

		for (n = 2; n < N; n <<= 1) {
			uint32_t m = (n < N / 2) ? n : (N - 1 - n);
			for (i = 1; i < m; i += 2) {
				j &= n - 1;
				j += i - 1;
				V_j = &V[j * s];
				Y = X + s;
				j = blockmix_xor(X, V_j, Y, r, ctx) & (NROM - 1);
				V_j = &VROM[j * s];
				X = Y + s;
				j = blockmix_xor(Y, V_j, X, r, ctx);
			}
		}
		n >>= 1;

		j &= n - 1;
		j += N - 2 - n;
		V_j = &V[j * s];
		Y = X + s;
		j = blockmix_xor(X, V_j, Y, r, ctx) & (NROM - 1);
		V_j = &VROM[j * s];
		blockmix_xor(Y, V_j, XY, r, ctx);
	} else if (flags & YESCRYPT_RW) {
//can't use flags___YESCRYPT_RW, smix1() may be called with flags = 0
		uint32_t n;
		salsa20_blk_t *V_j;

		blockmix(X, Y, r, ctx);
		X = Y + s;
		blockmix(Y, X, r, ctx);
		j = integerify(X, r);

		for (n = 2; n < N; n <<= 1) {
			uint32_t m = (n < N / 2) ? n : (N - 1 - n);
			for (i = 1; i < m; i += 2) {
				Y = X + s;
				j &= n - 1;
				j += i - 1;
				V_j = &V[j * s];
				j = blockmix_xor(X, V_j, Y, r, ctx);
				j &= n - 1;
				j += i;
				V_j = &V[j * s];
				X = Y + s;
				j = blockmix_xor(Y, V_j, X, r, ctx);
			}
		}
		n >>= 1;

		j &= n - 1;
		j += N - 2 - n;
		V_j = &V[j * s];
		Y = X + s;
		j = blockmix_xor(X, V_j, Y, r, ctx);
		j &= n - 1;
		j += N - 1 - n;
		V_j = &V[j * s];
		blockmix_xor(Y, V_j, XY, r, ctx);
	} else {
		N -= 2;
		do {
			blockmix_salsa8(X, Y, r);
			X = Y + s;
			blockmix_salsa8(Y, X, r);
			Y = X + s;
		} while ((N -= 2));

		blockmix_salsa8(X, Y, r);
		blockmix_salsa8(Y, XY, r);
	}

	for (i = 0; i < 2 * r; i++) {
		const salsa20_blk_t *src = &XY[i];
		salsa20_blk_t *tmp = &XY[s];
		salsa20_blk_t *dst = (salsa20_blk_t *)&B[i * 64];
		size_t k;
		for (k = 0; k < 16; k++)
			tmp->w[k] = SWAP_LE32(src->w[k]);
		salsa20_simd_unshuffle(tmp, dst);
	}
}

/**
 * smix2(B, r, N, Nloop, flags, V, NROM, VROM, XY, ctx):
 * Compute second loop of B = SMix_r(B, N).  The input B must be 128r bytes in
 * length; the temporary storage V must be 128rN bytes in length; the temporary
 * storage XY must be 256r bytes in length.  N must be a power of 2 and at
 * least 2.  Nloop must be even.  The array V must be aligned to a multiple of
 * 64 bytes, and arrays B and XY to a multiple of at least 16 bytes.
 */
#if DISABLE_NROM_CODE
#define smix2(B,r,N,Nloop,flags,V,NROM,VROM,XY,ctx) \
	smix2(B,r,N,Nloop,flags,V,XY,ctx)
#endif
static void smix2(uint8_t *B, size_t r, uint32_t N, uint64_t Nloop,
		uint32_t flags,
		salsa20_blk_t *V,
		uint32_t NROM, const salsa20_blk_t *VROM,
		salsa20_blk_t *XY,
		pwxform_ctx_t *ctx)
{
#if DISABLE_NROM_CODE
	uint32_t NROM = 0;
	const salsa20_blk_t *VROM = NULL;
#endif
	size_t s = 2 * r;
	salsa20_blk_t *X = XY, *Y = &XY[s];
	uint32_t i, j;

	if (Nloop == 0)
		return;

	for (i = 0; i < 2 * r; i++) {
		const salsa20_blk_t *src = (salsa20_blk_t *)&B[i * 64];
		salsa20_blk_t *tmp = Y;
		salsa20_blk_t *dst = &X[i];
		size_t k;
		for (k = 0; k < 16; k++)
			tmp->w[k] = SWAP_LE32(src->w[k]);
		salsa20_simd_shuffle(tmp, dst);
	}

	j = integerify(X, r) & (N - 1);

/*
 * Normally, VROM implies YESCRYPT_RW, but we check for these separately
 * because our SMix resets YESCRYPT_RW for the smix2() calls operating on the
 * entire V when p > 1.
 */
//and this is why bbox can't use flags___YESCRYPT_RW in this function
	if (VROM && (flags & YESCRYPT_RW)) {
		do {
			salsa20_blk_t *V_j = &V[j * s];
			const salsa20_blk_t *VROM_j;
			j = blockmix_xor_save(X, V_j, r, ctx) & (NROM - 1);
			VROM_j = &VROM[j * s];
			j = blockmix_xor(X, VROM_j, X, r, ctx) & (N - 1);
		} while (Nloop -= 2);
	} else if (VROM) {
		do {
			const salsa20_blk_t *V_j = &V[j * s];
			j = blockmix_xor(X, V_j, X, r, ctx) & (NROM - 1);
			V_j = &VROM[j * s];
			j = blockmix_xor(X, V_j, X, r, ctx) & (N - 1);
		} while (Nloop -= 2);
	} else if (flags & YESCRYPT_RW) {
		do {
			salsa20_blk_t *V_j = &V[j * s];
			j = blockmix_xor_save(X, V_j, r, ctx) & (N - 1);
			V_j = &V[j * s];
			j = blockmix_xor_save(X, V_j, r, ctx) & (N - 1);
		} while (Nloop -= 2);
	} else if (ctx) {
		do {
			const salsa20_blk_t *V_j = &V[j * s];
			j = blockmix_xor(X, V_j, X, r, ctx) & (N - 1);
			V_j = &V[j * s];
			j = blockmix_xor(X, V_j, X, r, ctx) & (N - 1);
		} while (Nloop -= 2);
	} else {
		do {
			const salsa20_blk_t *V_j = &V[j * s];
			j = blockmix_salsa8_xor(X, V_j, Y, r) & (N - 1);
			V_j = &V[j * s];
			j = blockmix_salsa8_xor(Y, V_j, X, r) & (N - 1);
		} while (Nloop -= 2);
	}

	for (i = 0; i < 2 * r; i++) {
		const salsa20_blk_t *src = &X[i];
		salsa20_blk_t *tmp = Y;
		salsa20_blk_t *dst = (salsa20_blk_t *)&B[i * 64];
		size_t k;
		for (k = 0; k < 16; k++)
			tmp->w[k] = SWAP_LE32(src->w[k]);
		salsa20_simd_unshuffle(tmp, dst);
	}
}

/**
 * p2floor(x):
 * Largest power of 2 not greater than argument.
 */
static uint64_t p2floor(uint64_t x)
{
	uint64_t y;
	while ((y = x & (x - 1)))
		x = y;
	return x;
}

/**
 * smix(B, r, N, p, t, flags, V, NROM, VROM, XY, S, passwd):
 * Compute B = SMix_r(B, N).  The input B must be 128rp bytes in length; the
 * temporary storage V must be 128rN bytes in length; the temporary storage
 * XY must be 256r or 256rp bytes in length (the larger size is required with
 * OpenMP-enabled builds).  N must be a power of 2 and at least 4.  The array V
 * must be aligned to a multiple of 64 bytes, and arrays B and XY to a multiple
 * of at least 16 bytes (aligning them to 64 bytes as well saves cache lines
 * and helps avoid false sharing in OpenMP-enabled builds when p > 1, but it
 * might also result in cache bank conflicts).
 */
#if DISABLE_NROM_CODE
#define smix(B,r,N,p,t,flags,V,NROM,VROM,XY,S,passwd) \
	smix(B,r,N,p,t,flags,V,XY,S,passwd)
#endif
static void smix(uint8_t *B, size_t r, uint32_t N, uint32_t p, uint32_t t,
		uint32_t flags,
		salsa20_blk_t *V,
		uint32_t NROM, const salsa20_blk_t *VROM,
		salsa20_blk_t *XY,
		uint8_t *S, uint8_t *passwd)
{
	size_t s = 2 * r;
	uint32_t Nchunk;
	uint64_t Nloop_all, Nloop_rw;
	uint32_t i;

	Nchunk = N / p;
	Nloop_all = Nchunk;
	if (flags___YESCRYPT_RW) {
		if (t <= 1) {
			if (t)
				Nloop_all *= 2; /* 2/3 */
			Nloop_all = (Nloop_all + 2) / 3; /* 1/3, round up */
		} else {
			Nloop_all *= t - 1;
		}
	} else if (t) {
		if (t == 1)
			Nloop_all += (Nloop_all + 1) / 2; /* 1.5, round up */
		Nloop_all *= t;
	}

	Nloop_rw = 0;
	if (flags___YESCRYPT_RW)
		Nloop_rw = Nloop_all / p;

	Nchunk &= ~(uint32_t)1; /* round down to even */
	Nloop_all++; Nloop_all &= ~(uint64_t)1; /* round up to even */
	Nloop_rw++; Nloop_rw &= ~(uint64_t)1; /* round up to even */

	for (i = 0; i < p; i++) {
		uint32_t Vchunk = i * Nchunk;
		uint32_t Np = (i < p - 1) ? Nchunk : (N - Vchunk);
		uint8_t *Bp = &B[128 * r * i];
		salsa20_blk_t *Vp = &V[Vchunk * s];
		salsa20_blk_t *XYp = XY;
		pwxform_ctx_t *ctx_i = NULL;
		if (flags___YESCRYPT_RW) {
			uint8_t *Si = S + i * Salloc;
			smix1(Bp, 1, Sbytes / 128, 0 /* no flags */,
				(salsa20_blk_t *)Si, 0, NULL, XYp, NULL);
			ctx_i = (pwxform_ctx_t *)(Si + Sbytes);
			ctx_i->S2 = Si;
			ctx_i->S1 = Si + Sbytes / 3;
			ctx_i->S0 = Si + Sbytes / 3 * 2;
			ctx_i->w = 0;
			if (i == 0)
				hmac_block(
					/* key,len: */ Bp + (128 * r - 64), 64,
					/* hash fn: */ sha256_begin,
					/* in,len: */  passwd, 32,
					/* outbuf: */  passwd
				);
		}
		smix1(Bp, r, Np, flags, Vp, NROM, VROM, XYp, ctx_i);
		smix2(Bp, r, p2floor(Np), Nloop_rw, flags, Vp,
			NROM, VROM, XYp, ctx_i);
	}

	if (Nloop_all > Nloop_rw) {
		for (i = 0; i < p; i++) {
			uint8_t *Bp = &B[128 * r * i];
			salsa20_blk_t *XYp = XY;
			pwxform_ctx_t *ctx_i = NULL;
			if (flags___YESCRYPT_RW) {
				uint8_t *Si = S + i * Salloc;
				ctx_i = (pwxform_ctx_t *)(Si + Sbytes);
			}
			smix2(Bp, r, N, Nloop_all - Nloop_rw,
				flags & (uint32_t)~YESCRYPT_RW,
				V, NROM, VROM, XYp, ctx_i);
		}
	}
}

/* Allocator code */

static void alloc_region(yescrypt_region_t *region, size_t size)
{
	uint8_t *base;
	int flags =
# ifdef MAP_NOCORE /* huh? */
		MAP_NOCORE |
# endif
		MAP_ANON | MAP_PRIVATE;

	base = mmap(NULL, size, PROT_READ | PROT_WRITE, flags, -1, 0);
	if (base == MAP_FAILED)
		bb_die_memory_exhausted();

#if defined(MADV_HUGEPAGE)
	/* Reduces mkpasswd qweRTY123@-+ '$y$jHT$123'
	 * (which allocates 4 Gbytes)
	 * run time from 10.543s to 5.635s
	 * Seen on linux-5.18.0.
	 */
	madvise(base, size, MADV_HUGEPAGE);
#endif
	//region->base = base;
	//region->base_size = size;
	region->aligned = base;
	region->aligned_size = size;
}

static void free_region(yescrypt_region_t *region)
{
	if (region->aligned)
		munmap(region->aligned, region->aligned_size);
	//region->base = NULL;
	//region->base_size = 0;
	region->aligned = NULL;
	region->aligned_size = 0;
}
/**
 * yescrypt_kdf_body(shared, local, passwd, passwdlen, salt, saltlen,
 *     flags, N, r, p, t, NROM, buf, buflen):
 * Compute scrypt(passwd[0 .. passwdlen - 1], salt[0 .. saltlen - 1], N, r,
 * p, buflen), or a revision of scrypt as requested by flags and shared, and
 * write the result into buf.
 *
 * shared and flags may request special modes as described in yescrypt.h.
 *
 * local is the thread-local data structure, allowing to preserve and reuse a
 * memory allocation across calls, thereby reducing its overhead.
 *
 * t controls computation time while not affecting peak memory usage.
 *
 * Return 0 on success; or -1 on error.
 *
 * This optimized implementation currently limits N to the range from 4 to
 * 2^31, but other implementations might not.
 */
static int yescrypt_kdf32_body(
		yescrypt_ctx_t *yctx,
		const uint8_t *passwd, size_t passwdlen,
		uint32_t flags, uint64_t N, uint32_t t,
		uint8_t *buf32)
{
#if !DISABLE_NROM_CODE
	const salsa20_blk_t *VROM;
#endif
	size_t B_size, V_size, XY_size, need;
	uint8_t *B, *S;
	salsa20_blk_t *V, *XY;
	struct {
		uint8_t sha256[32];
		uint8_t dk[32];
	} u;
#define sha256 u.sha256
#define dk     u.dk
	uint8_t *dkp = buf32;
	uint32_t r, p;

	/* Sanity-check parameters */
	switch (flags___YESCRYPT_MODE_MASK) {
	case 0: /* classic scrypt - can't have anything non-standard */
		if (flags || t || YCTX_param_NROM)
			goto out_EINVAL;
		break;
	case YESCRYPT_WORM:
		if (flags != YESCRYPT_WORM || YCTX_param_NROM)
			goto out_EINVAL;
		break;
	case YESCRYPT_RW:
		if (flags != (flags & YESCRYPT_KNOWN_FLAGS))
			goto out_EINVAL;
#if PWXsimple == 2 && PWXgather == 4 && Sbytes == 12288
		if ((flags & YESCRYPT_RW_FLAVOR_MASK) ==
		    (YESCRYPT_ROUNDS_6 | YESCRYPT_GATHER_4 |
		    YESCRYPT_SIMPLE_2 | YESCRYPT_SBOX_12K))
			break;
#else
#error "Unsupported pwxform settings"
#endif
		/* FALLTHRU */
	default:
		goto out_EINVAL;
	}

	r = YCTX_param_r;
	p = YCTX_param_p;
	if ((uint64_t)r * (uint64_t)p >= 1 << 30) {
		dbg("r * n >= 2^30");
		goto out_EINVAL;
	}
	if (N > UINT32_MAX) {
		dbg("N > 0x%lx", (long)UINT32_MAX);
		goto out_EINVAL;
	}
	if (N <= 3
	 || r < 1
	 || p < 1
	) {
		dbg("bad N, r or p");
		goto out_EINVAL;
	}
	if (r > SIZE_MAX / 256 / p
	 || N > SIZE_MAX / 128 / r
	) {
		/* 32-bit testcase: mkpasswd qweRTY123@-+ '$y$jHT$123'
		 * (works on 64-bit, needs buffer > 4Gbytes)
		 */
		dbg("r > SIZE_MAX / 256 / p? %c", "NY"[r > SIZE_MAX / 256 / p]);
		dbg("N > SIZE_MAX / 128 / r? %c", "NY"[N > SIZE_MAX / 128 / r]);
		goto out_EINVAL;
	}
	if (flags___YESCRYPT_RW) {
		/* p cannot be greater than SIZE_MAX/Salloc on 64-bit systems,
		   but it can on 32-bit systems.  */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wtype-limits"
		if (N / p <= 3 || p > SIZE_MAX / Salloc) {
			dbg("bad p:%ld", (long)p);
			goto out_EINVAL;
		}
#pragma GCC diagnostic pop
	}

#if !DISABLE_NROM_CODE
	VROM = NULL;
	if (YCTX_param_NROM)
		goto out_EINVAL;
#endif

	/* Allocate memory */
	V = NULL;
	V_size = (size_t)128 * r * N;
	need = V_size;
	B_size = (size_t)128 * r * p;
	need += B_size;
	if (need < B_size) {
		dbg("integer overflow at += B_size(%lu)", (long)B_size);
		goto out_EINVAL;
	}
	XY_size = (size_t)256 * r;
	need += XY_size;
	if (need < XY_size) {
		dbg("integer overflow at += XY_size(%lu)", (long)XY_size);
		goto out_EINVAL;
	}
	if (flags___YESCRYPT_RW) {
		size_t S_size = (size_t)Salloc * p;
		need += S_size;
		if (need < S_size) {
			dbg("integer overflow at += S_size(%lu)", (long)S_size);
			goto out_EINVAL;
		}
	}
	if (yctx->local->aligned_size < need) {
		free_region(yctx->local);
		alloc_region(yctx->local, need);
		dbg("allocated local:%lu 0x%lx", (long)need, (long)need);
		/* standard "j9T" params allocate 16Mbytes here */
	}
	if (flags & YESCRYPT_ALLOC_ONLY)
		return -3; /* expected "failure" */
	B = (uint8_t *)yctx->local->aligned;
	V = (salsa20_blk_t *)((uint8_t *)B + B_size);
	XY = (salsa20_blk_t *)((uint8_t *)V + V_size);
	S = NULL;
	if (flags___YESCRYPT_RW)
		S = (uint8_t *)XY + XY_size;

	if (flags) {
		hmac_block(
			/* key,len: */ (const void*)"yescrypt-prehash", (flags & YESCRYPT_PREHASH) ? 16 : 8,
			/* hash fn: */ sha256_begin,
			/* in,len: */  passwd, passwdlen,
			/* outbuf: */  sha256
		);
		passwd = sha256;
		passwdlen = sizeof(sha256);
	}

	PBKDF2_SHA256(passwd, passwdlen, yctx->salt, yctx->saltlen, 1, B, B_size);

	if (flags)
		memcpy(sha256, B, sizeof(sha256));

	if (p == 1 || (flags___YESCRYPT_RW)) {
		smix(B, r, N, p, t, flags, V, YCTX_param_NROM, VROM, XY, S, sha256);
	} else {
		uint32_t i;
		for (i = 0; i < p; i++) {
			smix(&B[(size_t)128 * r * i], r, N, 1, t, flags, V,
				YCTX_param_NROM, VROM, XY, NULL, NULL);
		}
	}

	dkp = buf32;
	if (flags && /*buflen:*/32 < sizeof(dk)) {
		PBKDF2_SHA256(passwd, passwdlen, B, B_size, 1, dk, sizeof(dk));
		dkp = dk;
	}

	PBKDF2_SHA256(passwd, passwdlen, B, B_size, 1, buf32, /*buflen:*/32);

	/*
	 * Except when computing classic scrypt, allow all computation so far
	 * to be performed on the client.  The final steps below match those of
	 * SCRAM (RFC 5802), so that an extension of SCRAM (with the steps so
	 * far in place of SCRAM's use of PBKDF2 and with SHA-256 in place of
	 * SCRAM's use of SHA-1) would be usable with yescrypt hashes.
	 */
	if (flags && !(flags & YESCRYPT_PREHASH)) {
		/* Compute ClientKey */
		hmac_block(
			/* key,len: */ dkp, sizeof(dk),
			/* hash fn: */ sha256_begin,
			/* in,len: */  "Client Key", 10,
			/* outbuf: */  sha256
		);
		/* Compute StoredKey */
		{
			size_t clen = /*buflen:*/32;
			if (clen > sizeof(dk))
				clen = sizeof(dk);
			if (sizeof(dk) != 32) { /* not true, optimize it out */
				sha256_block(sha256, sizeof(sha256), dk);
				memcpy(buf32, dk, clen);
			} else {
				sha256_block(sha256, sizeof(sha256), buf32);
			}
		}
	}

	explicit_bzero(&u, sizeof(u));

	/* Success! */
	return 0;

 out_EINVAL:
	//bbox does not need this: errno = EINVAL;
	return -1;
#undef sha256
#undef dk
}

/**
 * yescrypt_kdf(shared, local, passwd, passwdlen, salt, saltlen, params,
 *     buf, buflen):
 * Compute scrypt or its revision as requested by the parameters.  The inputs
 * to this function are the same as those for yescrypt_kdf_body() above, with
 * the addition of g, which controls hash upgrades (0 for no upgrades so far).
 */
static
int yescrypt_kdf32(
		yescrypt_ctx_t *yctx,
		const uint8_t *passwd, size_t passwdlen,
		uint8_t *buf32)
{
	uint32_t flags = YCTX_param_flags;
	uint64_t N = YCTX_param_N;
	uint32_t r = YCTX_param_r;
	uint32_t p = YCTX_param_p;
	uint32_t t = YCTX_param_t;
	uint32_t g = YCTX_param_g;
	uint8_t dk32[32];
	int retval;

	/* Support for hash upgrades has been temporarily removed */
	if (g) {
		//bbox does not need this: errno = EINVAL;
		return -1;
	}

	if ((flags___YESCRYPT_RW)
	 && p >= 1
	 && N / p >= 0x100
	 && N / p * r >= 0x20000
	) {
		if (yescrypt_kdf32_body(yctx,
		    passwd, passwdlen,
		    flags | YESCRYPT_ALLOC_ONLY, N, t,
		    buf32) != -3
		) {
			dbg("yescrypt_kdf32_body: not -3");
			return -1;
		}
		retval = yescrypt_kdf32_body(yctx,
				passwd, passwdlen,
				flags | YESCRYPT_PREHASH, N >> 6, 0,
				dk32);
		if (retval) {
			dbg("yescrypt_kdf32_body(PREHASH):%d", retval);
			return retval;
		}
		passwd = dk32;
		passwdlen = sizeof(dk32);
	}

	retval = yescrypt_kdf32_body(yctx,
			passwd, passwdlen,
			flags, N, t, buf32);

	explicit_bzero(dk32, sizeof(dk32));

	dbg("yescrypt_kdf32_body:%d", retval);
	return retval;
}
