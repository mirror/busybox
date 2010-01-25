/* vi: set sw=4 ts=4: */
/*
 * Unicode support routines.
 *
 * Copyright (C) 2009 Denys Vlasenko
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"
#include "unicode.h"

/* If it's not #defined as a constant in unicode.h... */
#ifndef unicode_status
uint8_t unicode_status;
#endif

/* This file is compiled only if FEATURE_ASSUME_UNICODE is on.
 * We check other options and decide whether to use libc support
 * via locale, or use our own logic:
 */

#if ENABLE_LOCALE_SUPPORT

/* Unicode support using libc locale support. */

void FAST_FUNC init_unicode(void)
{
	/* In unicode, this is a one character string */
	static const char unicode_0x394[] = { 0xce, 0x94, 0 };

	if (unicode_status != UNICODE_UNKNOWN)
		return;

	unicode_status = unicode_strlen(unicode_0x394) == 1 ? UNICODE_ON : UNICODE_OFF;
}

#else

/* Homegrown Unicode support. It knows only C and Unicode locales. */

# if ENABLE_FEATURE_CHECK_UNICODE_IN_ENV
void FAST_FUNC init_unicode(void)
{
	char *lang;

	if (unicode_status != UNICODE_UNKNOWN)
		return;

	unicode_status = UNICODE_OFF;
	lang = getenv("LANG");
	if (!lang || !(strstr(lang, ".utf") || strstr(lang, ".UTF")))
		return;
	unicode_status = UNICODE_ON;
}
# endif

static size_t wcrtomb_internal(char *s, wchar_t wc)
{
	int n, i;
	uint32_t v = wc;

	if (v <= 0x7f) {
		*s = v;
		return 1;
	}

	/* RFC 3629 says that Unicode ends at 10FFFF,
	 * but we cover entire 32 bits */

	/* 4000000-FFFFFFFF -> 111111tt 10tttttt 10zzzzzz 10zzyyyy 10yyyyxx 10xxxxxx */
	/* 200000-3FFFFFF -> 111110tt 10zzzzzz 10zzyyyy 10yyyyxx 10xxxxxx */
	/* 10000-1FFFFF -> 11110zzz 10zzyyyy 10yyyyxx 10xxxxxx */
	/* 800-FFFF -> 1110yyyy 10yyyyxx 10xxxxxx */
	/* 80-7FF -> 110yyyxx 10xxxxxx */

	/* How many bytes do we need? */
	n = 2;
	/* (0x80000000+ would result in n = 7, limiting n to 6) */
	while (v >= 0x800 && n < 6) {
		v >>= 5;
		n++;
	}
	/* Fill bytes n-1..1 */
	i = n;
	while (--i) {
		s[i] = (wc & 0x3f) | 0x80;
		wc >>= 6;
	}
	/* Fill byte 0 */
	s[0] = wc | (uint8_t)(0x3f00 >> n);
	return n;
}
size_t FAST_FUNC wcrtomb(char *s, wchar_t wc, mbstate_t *ps UNUSED_PARAM)
{
	if (unicode_status != UNICODE_ON) {
		*s = wc;
		return 1;
	}

	return wcrtomb_internal(s, wc);
}
size_t FAST_FUNC wcstombs(char *dest, const wchar_t *src, size_t n)
{
	size_t org_n = n;

	if (unicode_status != UNICODE_ON) {
		while (n) {
			wchar_t c = *src++;
			*dest++ = c;
			if (c == 0)
				break;
			n--;
		}
		return org_n - n;
	}

	while (n >= MB_CUR_MAX) {
		wchar_t wc = *src++;
		size_t len = wcrtomb_internal(dest, wc);

		if (wc == L'\0')
			return org_n - n;
		dest += len;
		n -= len;
	}
	while (n) {
		char tbuf[MB_CUR_MAX];
		wchar_t wc = *src++;
		size_t len = wcrtomb_internal(tbuf, wc);

		if (len > n)
			len = n;
		memcpy(dest, tbuf, len);
		if (wc == L'\0')
			return org_n - n;
		dest += len;
		n -= len;
	}
	return org_n - n;
}

static const char *mbstowc_internal(wchar_t *res, const char *src)
{
	int bytes;
	unsigned c = (unsigned char) *src++;

	if (c <= 0x7f) {
		*res = c;
		return src;
	}

	/* 80-7FF -> 110yyyxx 10xxxxxx */
	/* 800-FFFF -> 1110yyyy 10yyyyxx 10xxxxxx */
	/* 10000-1FFFFF -> 11110zzz 10zzyyyy 10yyyyxx 10xxxxxx */
	/* 200000-3FFFFFF -> 111110tt 10zzzzzz 10zzyyyy 10yyyyxx 10xxxxxx */
	/* 4000000-FFFFFFFF -> 111111tt 10tttttt 10zzzzzz 10zzyyyy 10yyyyxx 10xxxxxx */
	bytes = 0;
	do {
		c <<= 1;
		bytes++;
	} while ((c & 0x80) && bytes < 6);
	if (bytes == 1)
		return NULL;
	c = (uint8_t)(c) >> bytes;

	while (--bytes) {
		unsigned ch = (unsigned char) *src++;
		if ((ch & 0xc0) != 0x80) {
			return NULL;
		}
		c = (c << 6) + (ch & 0x3f);
	}

	/* TODO */
	/* Need to check that c isn't produced by overlong encoding */
	/* Example: 11000000 10000000 converts to NUL */
	/* 11110000 10000000 10000100 10000000 converts to 0x100 */
	/* correct encoding: 11000100 10000000 */
	if (c <= 0x7f) { /* crude check */
		return NULL;
		//or maybe 0xfffd; /* replacement character */
	}

	*res = c;
	return src;
}
size_t FAST_FUNC mbstowcs(wchar_t *dest, const char *src, size_t n)
{
	size_t org_n = n;

	if (unicode_status != UNICODE_ON) {
		while (n) {
			unsigned char c = *src++;

			if (dest)
				*dest++ = c;
			if (c == 0)
				break;
			n--;
		}
		return org_n - n;
	}

	while (n) {
		wchar_t wc;
		src = mbstowc_internal(&wc, src);
		if (src == NULL) /* error */
			return (size_t) -1L;
		if (dest)
			*dest++ = wc;
		if (wc == 0) /* end-of-string */
			break;
		n--;
	}

	return org_n - n;
}

#include "unicode_wcwidth.c"

int FAST_FUNC iswspace(wint_t wc)
{
	return (unsigned)wc <= 0x7f && isspace(wc);
}

int FAST_FUNC iswalnum(wint_t wc)
{
	return (unsigned)wc <= 0x7f && isalnum(wc);
}

int FAST_FUNC iswpunct(wint_t wc)
{
	return (unsigned)wc <= 0x7f && ispunct(wc);
}

#endif /* Homegrown Unicode support */


/* The rest is mostly same for libc and for "homegrown" support */

size_t FAST_FUNC unicode_strlen(const char *string)
{
	size_t width = mbstowcs(NULL, string, INT_MAX);
	if (width == (size_t)-1L)
		return strlen(string);
	return width;
}

char* FAST_FUNC unicode_cut_nchars(unsigned width, const char *src)
{
	char *dst;
	unsigned dst_len;

	if (unicode_status != UNICODE_ON)
		return xasprintf("%-*.*s", width, width, src);

	dst = NULL;
	dst_len = 0;
	while (1) {
		int w;
		wchar_t wc;

		dst = xrealloc(dst, dst_len + 2 * MB_CUR_MAX);
#if ENABLE_LOCALE_SUPPORT
		{
			mbstate_t mbst = { 0 };
			ssize_t rc = mbsrtowcs(&wc, &src, 1, &mbst);
			if (rc <= 0) /* error, or end-of-string */
				break;
		}
#else
		src = mbstowc_internal(&wc, src);
		if (!src || wc == 0) /* error, or end-of-string */
			break;
#endif
		w = wcwidth(wc);
		if (w < 0) /* non-printable wchar */
			break;
		width -= w;
		if ((int)width < 0) { /* string is longer than width */
			width += w;
			while (width) {
				dst[dst_len++] = ' ';
				width--;
			}
			break;
		}
#if ENABLE_LOCALE_SUPPORT
		{
			mbstate_t mbst = { 0 };
			dst_len += wcrtomb(&dst[dst_len], wc, &mbst);
		}
#else
		dst_len += wcrtomb_internal(&dst[dst_len], wc);
#endif
	}
	dst[dst_len] = '\0';
	return dst;
}

unsigned FAST_FUNC unicode_padding_to_width(unsigned width, const char *src)
{
	if (unicode_status != UNICODE_ON) {
		return width - strnlen(src, width);
	}

	while (1) {
		int w;
		wchar_t wc;

#if ENABLE_LOCALE_SUPPORT
		{
			mbstate_t mbst = { 0 };
			ssize_t rc = mbsrtowcs(&wc, &src, 1, &mbst);
			if (rc <= 0) /* error, or end-of-string */
				return width;
		}
#else
		src = mbstowc_internal(&wc, src);
		if (!src || wc == 0) /* error, or end-of-string */
			return width;
#endif
		w = wcwidth(wc);
		if (w < 0) /* non-printable wchar */
			return width;
		width -= w;
		if ((int)width <= 0) /* string is longer than width */
			return 0;
	}
}
