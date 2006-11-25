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


/* Provides extern inline definitions of functions */
/* (useful for mapping them to the type of the same width) */
#define DEFINE_EQUIV_STR_CONV(narrow, N, W, UN, UW) \
\
extern inline \
unsigned narrow xstrto##UN##_range_sfx(const char *str, int b, unsigned narrow l, unsigned narrow u, const struct suffix_mult *sfx) \
{ return xstrto##UW##_range_sfx(str, b, l, u, sfx); } \
extern inline \
unsigned narrow xstrto##UN##_range(const char *str, int b, unsigned narrow l, unsigned narrow u) \
{ return xstrto##UW##_range(str, b, l, u); } \
extern inline \
unsigned narrow xstrto##UN##_sfx(const char *str, int b, const struct suffix_mult *sfx) \
{ return xstrto##UW##_sfx(str, b, sfx); } \
extern inline \
unsigned narrow xstrto##UN(const char *str, int b) \
{ return xstrto##UW(str, b); } \
extern inline \
unsigned narrow xato##UN##_range_sfx(const char *str, unsigned narrow l, unsigned narrow u, const struct suffix_mult *sfx) \
{ return xato##UW##_range_sfx(str, l, u, sfx); } \
extern inline \
unsigned narrow xato##UN##_range(const char *str, unsigned narrow l, unsigned narrow u) \
{ return xato##UW##_range(str, l, u); } \
extern inline \
unsigned narrow xato##UN##_sfx(const char *str, const struct suffix_mult *sfx) \
{ return xato##UW##_sfx(str, sfx); } \
extern inline \
unsigned narrow xato##UN(const char *str) \
{ return xato##UW(str); } \
extern inline \
narrow xstrto##N##_range_sfx(const char *str, int b, narrow l, narrow u, const struct suffix_mult *sfx) \
{ return xstrto##W##_range_sfx(str, b, l, u, sfx); } \
extern inline \
narrow xstrto##N##_range(const char *str, int b, narrow l, narrow u) \
{ return xstrto##W##_range(str, b, l, u); } \
extern inline \
narrow xato##N##_range_sfx(const char *str, narrow l, narrow u, const struct suffix_mult *sfx) \
{ return xato##W##_range_sfx(str, l, u, sfx); } \
extern inline \
narrow xato##N##_range(const char *str, narrow l, narrow u) \
{ return xato##W##_range(str, l, u); } \
extern inline \
narrow xato##N##_sfx(const char *str, const struct suffix_mult *sfx) \
{ return xato##W##_sfx(str, sfx); } \
extern inline \
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
