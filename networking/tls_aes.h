/*
 * Copyright (C) 2017 Denys Vlasenko
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 *
 * Selected few declarations for AES.
 */

int32 psAesInitKey(const unsigned char *key, uint32 keylen, psAesKey_t *skey);
void psAesEncryptBlock(const unsigned char *pt, unsigned char *ct,
				psAesKey_t *skey);
void psAesDecryptBlock(const unsigned char *ct, unsigned char *pt,
				psAesKey_t *skey);

int32 psAesInit(psCipherContext_t *ctx, unsigned char *IV,
				  unsigned char *key, uint32 keylen);
int32 psAesEncrypt(psCipherContext_t *ctx, unsigned char *pt,
					 unsigned char *ct, uint32 len);
int32 psAesDecrypt(psCipherContext_t *ctx, unsigned char *ct,
					 unsigned char *pt, uint32 len);
