/* vi: set sw=4 ts=4: */
/*
 * ascii-to-numbers implementations for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

/* Provides extern declarations of functions */
#define DECLARE_STR_CONV(type, T, UT) \
\
unsigned type xstrto##UT##_range_sfx(const char *str, int b, unsigned type l, unsigned type u, const struct suffix_mult *sfx); \
unsigned type xstrto##UT##_range(const char *str, int b, unsigned type l, unsigned type u); \
unsigned type xstrto##UT##_sfx(const char *str, int b, const struct suffix_mult *sfx); \
unsigned type xstrto##UT(const char *str, int b); \
unsigned type xato##UT##_range_sfx(const char *str, unsigned type l, unsigned type u, const struct suffix_mult *sfx); \
unsigned type xato##UT##_range(const char *str, unsigned type l, unsigned type u); \
unsigned type xato##UT##_sfx(const char *str, const struct suffix_mult *sfx); \
unsigned type xato##UT(const char *str); \
type xstrto##T##_range_sfx(const char *str, int b, type l, type u, const struct suffix_mult *sfx) ;\
type xstrto##T##_range(const char *str, int b, type l, type u); \
type xato##T##_range_sfx(const char *str, type l, type u, const struct suffix_mult *sfx); \
type xato##T##_range(const char *str, type l, type u); \
type xato##T##_sfx(const char *str, const struct suffix_mult *sfx); \
type xato##T(const char *str); \

/* Unsigned long long functions always exist */
DECLARE_STR_CONV(long long, ll, ull)


/* Provides inline definitions of functions */
/* (useful for mapping them to the type of the same width) */
#define DEFINE_EQUIV_STR_CONV(narrow, N, W, UN, UW) \
\
static ATTRIBUTE_ALWAYS_INLINE \
unsigned narrow xstrto##UN##_range_sfx(const char *str, int b, unsigned narrow l, unsigned narrow u, const struct suffix_mult *sfx) \
{ return xstrto##UW##_range_sfx(str, b, l, u, sfx); } \
static ATTRIBUTE_ALWAYS_INLINE \
unsigned narrow xstrto##UN##_range(const char *str, int b, unsigned narrow l, unsigned narrow u) \
{ return xstrto##UW##_range(str, b, l, u); } \
static ATTRIBUTE_ALWAYS_INLINE \
unsigned narrow xstrto##UN##_sfx(const char *str, int b, const struct suffix_mult *sfx) \
{ return xstrto##UW##_sfx(str, b, sfx); } \
static ATTRIBUTE_ALWAYS_INLINE \
unsigned narrow xstrto##UN(const char *str, int b) \
{ return xstrto##UW(str, b); } \
static ATTRIBUTE_ALWAYS_INLINE \
unsigned narrow xato##UN##_range_sfx(const char *str, unsigned narrow l, unsigned narrow u, const struct suffix_mult *sfx) \
{ return xato##UW##_range_sfx(str, l, u, sfx); } \
static ATTRIBUTE_ALWAYS_INLINE \
unsigned narrow xato##UN##_range(const char *str, unsigned narrow l, unsigned narrow u) \
{ return xato##UW##_range(str, l, u); } \
static ATTRIBUTE_ALWAYS_INLINE \
unsigned narrow xato##UN##_sfx(const char *str, const struct suffix_mult *sfx) \
{ return xato##UW##_sfx(str, sfx); } \
static ATTRIBUTE_ALWAYS_INLINE \
unsigned narrow xato##UN(const char *str) \
{ return xato##UW(str); } \
static ATTRIBUTE_ALWAYS_INLINE \
narrow xstrto##N##_range_sfx(const char *str, int b, narrow l, narrow u, const struct suffix_mult *sfx) \
{ return xstrto##W##_range_sfx(str, b, l, u, sfx); } \
static ATTRIBUTE_ALWAYS_INLINE \
narrow xstrto##N##_range(const char *str, int b, narrow l, narrow u) \
{ return xstrto##W##_range(str, b, l, u); } \
static ATTRIBUTE_ALWAYS_INLINE \
narrow xato##N##_range_sfx(const char *str, narrow l, narrow u, const struct suffix_mult *sfx) \
{ return xato##W##_range_sfx(str, l, u, sfx); } \
static ATTRIBUTE_ALWAYS_INLINE \
narrow xato##N##_range(const char *str, narrow l, narrow u) \
{ return xato##W##_range(str, l, u); } \
static ATTRIBUTE_ALWAYS_INLINE \
narrow xato##N##_sfx(const char *str, const struct suffix_mult *sfx) \
{ return xato##W##_sfx(str, sfx); } \
static ATTRIBUTE_ALWAYS_INLINE \
narrow xato##N(const char *str) \
{ return xato##W(str); } \

/* If long == long long, then just map them one-to-one */
#if ULONG_MAX == ULLONG_MAX
DEFINE_EQUIV_STR_CONV(long, l, ll, ul, ull)
#else
/* Else provide extern defs */
DECLARE_STR_CONV(long, l, ul)
#endif

/* Same for int -> [long] long */
#if UINT_MAX == ULLONG_MAX
DEFINE_EQUIV_STR_CONV(int, i, ll, u, ull)
#elif UINT_MAX == ULONG_MAX
DEFINE_EQUIV_STR_CONV(int, i, l, u, ul)
#else
DECLARE_STR_CONV(int, i, u)
#endif

/* Specialized */

int BUG_xatou32_unimplemented(void);
static ATTRIBUTE_ALWAYS_INLINE uint32_t xatou32(const char *numstr)
{
	if (UINT_MAX == 0xffffffff)
		return xatou(numstr);
	if (ULONG_MAX == 0xffffffff)
		return xatoul(numstr);
	return BUG_xatou32_unimplemented();
}

/* Non-aborting kind of convertors */

unsigned long long bb_strtoull(const char *arg, char **endp, int base);
long long bb_strtoll(const char *arg, char **endp, int base);

#if ULONG_MAX == ULLONG_MAX
static ATTRIBUTE_ALWAYS_INLINE
unsigned long bb_strtoul(const char *arg, char **endp, int base)
{ return bb_strtoull(arg, endp, base); }
static ATTRIBUTE_ALWAYS_INLINE
long bb_strtol(const char *arg, char **endp, int base)
{ return bb_strtoll(arg, endp, base); }
#else
unsigned long bb_strtoul(const char *arg, char **endp, int base);
long bb_strtol(const char *arg, char **endp, int base);
#endif

#if UINT_MAX == ULLONG_MAX
static ATTRIBUTE_ALWAYS_INLINE
unsigned bb_strtou(const char *arg, char **endp, int base)
{ return bb_strtoull(arg, endp, base); }
static ATTRIBUTE_ALWAYS_INLINE
int bb_strtoi(const char *arg, char **endp, int base)
{ return bb_strtoll(arg, endp, base); }
#elif UINT_MAX == ULONG_MAX
static ATTRIBUTE_ALWAYS_INLINE
unsigned bb_strtou(const char *arg, char **endp, int base)
{ return bb_strtoul(arg, endp, base); }
static ATTRIBUTE_ALWAYS_INLINE
int bb_strtoi(const char *arg, char **endp, int base)
{ return bb_strtol(arg, endp, base); }
#else
unsigned bb_strtou(const char *arg, char **endp, int base);
int bb_strtoi(const char *arg, char **endp, int base);
#endif

int BUG_bb_strtou32_unimplemented(void);
static ATTRIBUTE_ALWAYS_INLINE
uint32_t bb_strtou32(const char *arg, char **endp, int base)
{
	if (sizeof(uint32_t) == sizeof(unsigned))
		return bb_strtou(arg, endp, base);
	if (sizeof(uint32_t) == sizeof(unsigned long))
		return bb_strtoul(arg, endp, base);
	return BUG_bb_strtou32_unimplemented();
}

/* Floating point */

/* double bb_strtod(const char *arg, char **endp); */
