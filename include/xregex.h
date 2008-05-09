/* vi: set sw=4 ts=4: */
/*
 * Busybox xregcomp utility routine.  This isn't in libbb.h because the
 * C library we're linking against may not support regex.h.
 *
 * Based in part on code from sash, Copyright (c) 1999 by David I. Bell
 * Permission has been granted to redistribute this code under the GPL.
 *
 * Licensed under GPLv2 or later, see file License in this tarball for details.
 */
#ifndef __BB_REGEX__
#define __BB_REGEX__

#include <regex.h>

#if __GNUC_PREREQ(4,1)
# pragma GCC visibility push(hidden)
#endif

char* regcomp_or_errmsg(regex_t *preg, const char *regex, int cflags);
void xregcomp(regex_t *preg, const char *regex, int cflags);

#if __GNUC_PREREQ(4,1)
# pragma GCC visibility pop
#endif

#endif
