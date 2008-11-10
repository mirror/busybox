/* vi: set sw=4 ts=4: */
/*
 * Utility routine.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#if ENABLE_USE_BB_CRYPT

/*
 * DES and MD5 crypt implementations are taken from uclibc.
 * They were modified to not use static buffers.
 */

/* Used by pw_encrypt_XXX.c */
static const uint8_t ascii64[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static char*
to64(char *s, unsigned v, int n)
{
	while (--n >= 0) {
		*s++ = ascii64[v & 0x3f];
		v >>= 6;
	}
	return s;
}

#include "pw_encrypt_des.c"
#include "pw_encrypt_md5.c"
#if ENABLE_USE_BB_CRYPT_SHA
#include "pw_encrypt_sha.c"
#endif

/* Other advanced crypt ids (TODO?): */
/* $2$ or $2a$: Blowfish */

static struct const_des_ctx *des_cctx;
static struct des_ctx *des_ctx;

/* my_crypt returns malloc'ed data */
static char *my_crypt(const char *key, const char *salt)
{
	/* MD5 or SHA? */
	if (salt[0] == '$' && salt[1] && salt[2] == '$') {
		if (salt[1] == '1')
			return md5_crypt(xzalloc(MD5_OUT_BUFSIZE), (unsigned char*)key, (unsigned char*)salt);
#if ENABLE_USE_BB_CRYPT_SHA
		if (salt[1] == '5' || salt[1] == '6')
			return sha_crypt((char*)key, (char*)salt);
#endif
	}

	if (!des_cctx)
		des_cctx = const_des_init();
	des_ctx = des_init(des_ctx, des_cctx);
	return des_crypt(des_ctx, xzalloc(DES_OUT_BUFSIZE), (unsigned char*)key, (unsigned char*)salt);
}

/* So far nobody wants to have it public */
static void my_crypt_cleanup(void)
{
	free(des_cctx);
	free(des_ctx);
	des_cctx = NULL;
	des_ctx = NULL;
}

char* FAST_FUNC pw_encrypt(const char *clear, const char *salt, int cleanup)
{
	char *encrypted;

	encrypted = my_crypt(clear, salt);

	if (cleanup)
		my_crypt_cleanup();

	return encrypted;
}

#else /* if !ENABLE_USE_BB_CRYPT */

char* FAST_FUNC pw_encrypt(const char *clear, const char *salt, int cleanup)
{
	return xstrdup(crypt(clear, salt));
}

#endif
