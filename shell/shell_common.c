/* vi: set sw=4 ts=4: */
/*
 * Adapted from ash applet code
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Copyright (c) 1989, 1991, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1997-2005 Herbert Xu <herbert@gondor.apana.org.au>
 * was re-ported from NetBSD and debianized.
 *
 * Copyright (c) 2010 Denys Vlasenko
 * Split from ash.c
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */
#include "libbb.h"
#include "shell_common.h"

const char defifsvar[] ALIGN1 = "IFS= \t\n";


int FAST_FUNC is_well_formed_var_name(const char *s, char terminator)
{
	if (!s || !(isalpha(*s) || *s == '_'))
		return 0;

	do
		s++;
	while (isalnum(*s) || *s == '_');

	return *s == terminator;
}
