/*
 * Copyright (C) 2025 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//kbuild:lib-$(CONFIG_TLS) += hash_hmac.o
//kbuild:lib-$(CONFIG_USE_BB_CRYPT_YES) += hash_hmac.o

#include "libbb.h"

// RFC 2104:
// HMAC(key, text) based on a hash H (say, sha256) is:
// ipad = [0x36 x INSIZE]
// opad = [0x5c x INSIZE]
// HMAC(key, text) = H((key XOR opad) + H((key XOR ipad) + text))
//
// H(key XOR opad) and H(key XOR ipad) can be precomputed
// if we often need HMAC hmac with the same key.
//
// text is often given in disjoint pieces.
void FAST_FUNC hmac_begin(hmac_ctx_t *ctx, const uint8_t *key, unsigned key_size, md5sha_begin_func *begin)
{
#if HMAC_ONLY_SHA256
#define begin sha256_begin
#endif
	uint8_t key_xor_ipad[SHA2_INSIZE];
	uint8_t key_xor_opad[SHA2_INSIZE];
	unsigned i;

	// "The authentication key can be of any length up to INSIZE, the
	// block length of the hash function.  Applications that use keys longer
	// than INSIZE bytes will first hash the key using H and then use the
	// resultant OUTSIZE byte string as the actual key to HMAC."
	if (key_size > SHA2_INSIZE) {
		uint8_t tempkey[SHA1_OUTSIZE < SHA256_OUTSIZE ? SHA256_OUTSIZE : SHA1_OUTSIZE];
		/* use ctx->hashed_key_xor_ipad as scratch ctx */
		begin(&ctx->hashed_key_xor_ipad);
		md5sha_hash(&ctx->hashed_key_xor_ipad, key, key_size);
		key_size = sha_end(&ctx->hashed_key_xor_ipad, tempkey);
		key = tempkey;
	}

	for (i = 0; i < key_size; i++) {
		key_xor_ipad[i] = key[i] ^ 0x36;
		key_xor_opad[i] = key[i] ^ 0x5c;
	}
	for (; i < SHA2_INSIZE; i++) {
		key_xor_ipad[i] = 0x36;
		key_xor_opad[i] = 0x5c;
	}

	begin(&ctx->hashed_key_xor_ipad);
	begin(&ctx->hashed_key_xor_opad);
	md5sha_hash(&ctx->hashed_key_xor_ipad, key_xor_ipad, SHA2_INSIZE);
	md5sha_hash(&ctx->hashed_key_xor_opad, key_xor_opad, SHA2_INSIZE);
}
#undef begin

unsigned FAST_FUNC hmac_end(hmac_ctx_t *ctx, uint8_t *out)
{
	unsigned len = sha_end(&ctx->hashed_key_xor_ipad, out);
	/* out = H((key XOR opad) + out) */
	md5sha_hash(&ctx->hashed_key_xor_opad, out, len);
	return sha_end(&ctx->hashed_key_xor_opad, out);
}

unsigned FAST_FUNC hmac_block(const uint8_t *key, unsigned key_size, md5sha_begin_func *begin, const void *in, unsigned sz, uint8_t *out)
{
	hmac_ctx_t ctx;
	hmac_begin(&ctx, key, key_size, begin);
	hmac_hash(&ctx, in, sz);
	return hmac_end(&ctx, out);
}

/* TLS helpers */

void FAST_FUNC hmac_hash_v(
		hmac_ctx_t *ctx,
		va_list va)
{
	uint8_t *in;

	/* ctx->hashed_key_xor_ipad contains unclosed "H((key XOR ipad) +" state */
	/* ctx->hashed_key_xor_opad contains unclosed "H((key XOR opad) +" state */

	/* calculate out = H((key XOR ipad) + text) */
	while ((in = va_arg(va, uint8_t*)) != NULL) {
		unsigned size = va_arg(va, unsigned);
		md5sha_hash(&ctx->hashed_key_xor_ipad, in, size);
	}
}

/* Using HMAC state, make a copy of it (IOW: without affecting this state!)
 * hash in the list of (ptr,size) blocks, and finish the HMAC to out[] buffer.
 * This function is useful for TLS PRF.
 */
unsigned FAST_FUNC hmac_peek_hash(hmac_ctx_t *ctx, uint8_t *out, ...)
{
	hmac_ctx_t tmpctx = *ctx; /* struct copy */
	va_list va;

	va_start(va, out);
	hmac_hash_v(&tmpctx, va);
	va_end(va);

	return hmac_end(&tmpctx, out);
}
