/* vi: set sw=4 ts=4: */
/*
 * Utility routine.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include <crypt.h>

char *pw_encrypt(const char *clear, const char *salt)
{
	static char cipher[128];
	char *cp;

#if 0 /* was CONFIG_FEATURE_SHA1_PASSWORDS, but there is no such thing??? */
	if (strncmp(salt, "$2$", 3) == 0) {
		return sha1_crypt(clear);
	}
#endif
	cp = (char *) crypt(clear, salt);
	/* if crypt (a nonstandard crypt) returns a string too large,
	   truncate it so we don't overrun buffers and hope there is
	   enough security in what's left */
	safe_strncpy(cipher, cp, sizeof(cipher));
	return cipher;
}
