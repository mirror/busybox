/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2025 Denys Vlasenko
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//kbuild:lib-y += hash_sha256_block.o
#include "libbb.h"

void FAST_FUNC
sha256_block(const void *in, size_t len, uint8_t hash[32])
{
	sha256_ctx_t ctx;
	sha256_begin(&ctx);
	sha256_hash(&ctx, in, len);
	sha256_end(&ctx, hash);
}
