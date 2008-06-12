/* vi: set sw=4 ts=4: */
/*
 * Utility routine.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

/*
 * DES and MD5 crypt implementations are taken from uclibc.
 * They were modified to not use static buffers.
 * Comparison with uclibc (before uclibc had 70k staic buffers reinstated):
 *   text    data     bss     dec     hex filename
 * 759909     604    6684  767197   bb4dd busybox_old
 * 759579     604    6684  766867   bb393 busybox_unstripped
 */
/* Common for them */
static const uint8_t ascii64[] = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
#include "pw_encrypt_des.c"
#include "pw_encrypt_md5.c"


static struct const_des_ctx *des_cctx;
static struct des_ctx *des_ctx;

/* my_crypt returns malloc'ed data */
static char *my_crypt(const char *key, const char *salt)
{
	/* First, check if we are supposed to be using the MD5 replacement
	 * instead of DES...  */
	if (salt[0] == '$' && salt[1] == '1' && salt[2] == '$') {
		return md5_crypt(xzalloc(MD5_OUT_BUFSIZE), (unsigned char*)key, (unsigned char*)salt);
	}

	{
		if (!des_cctx)
			des_cctx = const_des_init();
		des_ctx = des_init(des_ctx, des_cctx);
		return des_crypt(des_ctx, xzalloc(DES_OUT_BUFSIZE), (unsigned char*)key, (unsigned char*)salt);
	}
}

/* So far nobody wants to have it public */
static void my_crypt_cleanup(void)
{
	free(des_cctx);
	free(des_ctx);
	des_cctx = NULL;
	des_ctx = NULL;
}

char *pw_encrypt(const char *clear, const char *salt, int cleanup)
{
	char *encrypted;

#if 0 /* was CONFIG_FEATURE_SHA1_PASSWORDS, but there is no such thing??? */
	if (strncmp(salt, "$2$", 3) == 0) {
		return sha1_crypt(clear);
	}
#endif

	encrypted = my_crypt(clear, salt);

	if (cleanup)
		my_crypt_cleanup();

	return encrypted;
}
