/* vi: set sw=4 ts=4: */
/*
 * Utility routine.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <string.h>
#include <crypt.h>
#include "libbb.h"


extern char *pw_encrypt(const char *clear, const char *salt)
{
	static char cipher[128];
	char *cp;

#ifdef CONFIG_FEATURE_SHA1_PASSWORDS
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

