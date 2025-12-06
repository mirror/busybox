/*
 * Utility routines.
 *
 * Copyright (C) 2025 by Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "yescrypt/alg-yescrypt.h"

static char *
yes_crypt(const char *passwd, const char *salt_data)
{
	/* prefix, '$', hash, NUL */
	char buf[YESCRYPT_PREFIX_LEN + 1 + YESCRYPT_HASH_LEN + 1];
	char *retval;

	retval = yescrypt_r(
			(const uint8_t *)passwd, strlen(passwd),
			(const uint8_t *)salt_data,
			buf, sizeof(buf));
	/* The returned value is either buf[], or NULL on error */

	return xstrdup(retval);
}
