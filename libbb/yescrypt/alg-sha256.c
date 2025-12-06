/*-
 * Copyright 2005-2016 Colin Percival
 * Copyright 2016-2018,2021 Alexander Peslyak
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
 */

/**
 * PBKDF2_SHA256(passwd, passwdlen, salt, saltlen, c, buf, dkLen):
 * Compute PBKDF2(passwd, salt, c, dkLen) using HMAC-SHA256 as the PRF, and
 * write the output to buf.  The value dkLen must be at most 32 * (2^32 - 1).
 */
static void
PBKDF2_SHA256(const uint8_t *passwd, size_t passwdlen,
		const uint8_t *salt, size_t saltlen,
		uint64_t c, uint8_t *buf, size_t dkLen)
{
	hmac_ctx_t Phctx, PShctx;
	uint32_t i;

	/* Compute HMAC state after processing P. */
	hmac_begin(&Phctx, passwd, passwdlen, sha256_begin);

	/* Compute HMAC state after processing P and S. */
	PShctx = Phctx;
	hmac_hash(&PShctx, salt, saltlen);

	/* Iterate through the blocks. */
	for (i = 0; dkLen != 0; ) {
		long U[32 / sizeof(long)];
		long T[32 / sizeof(long)];
// Do not make these ^^ uint64_t[]. Keep them long[].
// Even though the XORing loop below is optimized out,
// gcc is not smart enough to realize that 64-bit alignment of the stack
// is no longer useful, and generates ~50 more bytes of code on i386...
		uint32_t ivec;
		size_t clen;
		int k;

		/* Generate INT(i). */
		i++;
		ivec = SWAP_BE32(i);

		/* Compute U_1 = PRF(P, S || INT(i)). */
		hmac_peek_hash(&PShctx, (void*)T, &ivec, 4, NULL);
//TODO: the above is a vararg function, might incur some ABI pain
//does libbb need a non-vararg version with just one (buf,len)?

		if (c > 1) {
//in yescrypt, c is always 1, so this if() branch is optimized out
			uint64_t j;
			/* T_i = U_1 ... */
			memcpy(U, T, 32);
			for (j = 2; j <= c; j++) {
				/* Compute U_j. */
				hmac_peek_hash(&Phctx, (void*)U, U, 32, NULL);
				/* ... xor U_j ... */
				for (k = 0; k < 32 / sizeof(long); k++)
					T[k] ^= U[k];
				//TODO: xorbuf32_aligned_long(T, U);
			}
		}

		/* Copy as many bytes as necessary into buf. */
		clen = dkLen;
		if (clen > 32)
			clen = 32;
		buf = mempcpy(buf, T, clen);
		dkLen -= clen;
	}
}
