/* vi: set sw=4 ts=4: */
/*
 * ascii-to-numbers implementations for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#define type long long
#define xstrtou(rest) xstrtoull##rest
#define xstrto(rest) xstrtoll##rest
#define xatou(rest) xatoull##rest
#define xato(rest) xatoll##rest
#define XSTR_UTYPE_MAX ULLONG_MAX
#define XSTR_TYPE_MAX LLONG_MAX
#define XSTR_TYPE_MIN LLONG_MIN
#define XSTR_STRTOU strtoull
#include "xatonum_template.c"
#undef type
#undef xstrtou
#undef xstrto
#undef xatou
#undef xato
#undef XSTR_UTYPE_MAX
#undef XSTR_TYPE_MAX
#undef XSTR_TYPE_MIN
#undef XSTR_STRTOU

#if ULONG_MAX != ULLONG_MAX
#define type long
#define xstrtou(rest) xstrtoul##rest
#define xstrto(rest) xstrtol##rest
#define xatou(rest) xatoul##rest
#define xato(rest) xatol##rest
#define XSTR_UTYPE_MAX ULONG_MAX
#define XSTR_TYPE_MAX LONG_MAX
#define XSTR_TYPE_MIN LONG_MIN
#define XSTR_STRTOU strtoul
#include "xatonum_template.c"
#undef type
#undef xstrtou
#undef xstrto
#undef xatou
#undef xato
#undef XSTR_UTYPE_MAX
#undef XSTR_TYPE_MAX
#undef XSTR_TYPE_MIN
#undef XSTR_STRTOU
#endif

#if UINT_MAX != ULONG_MAX
#define type int
#define xstrtou(rest) xstrtou##rest
#define xstrto(rest) xstrtoi##rest
#define xatou(rest) xatou##rest
#define xato(rest) xatoi##rest
#define XSTR_UTYPE_MAX UINT_MAX
#define XSTR_TYPE_MAX INT_MAX
#define XSTR_TYPE_MIN INT_MIN
#define XSTR_STRTOU strtoul
#include "xatonum_template.c"
#undef type
#undef xstrtou
#undef xstrto
#undef xatou
#undef xato
#undef XSTR_UTYPE_MAX
#undef XSTR_TYPE_MAX
#undef XSTR_TYPE_MIN
#undef XSTR_STRTOU
#endif

/* A few special cases */

int xatoi_u(const char *numstr)
{
	return xatoul_range(numstr, 0, INT_MAX);
}

uint32_t xatou32(const char *numstr)
{
	return xatoul_range(numstr, 0, 0xffffffff);
}

uint16_t xatou16(const char *numstr)
{
	return xatoul_range(numstr, 0, 0xffff);
}
