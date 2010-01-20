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

/* If it's not a constant... */
#ifndef unicode_status
uint8_t unicode_status;
#endif

size_t FAST_FUNC bb_mbstrlen(const char *string)
{
	size_t width = mbstowcs(NULL, string, INT_MAX);
	if (width == (size_t)-1L)
		return strlen(string);
	return width;
}

#if ENABLE_LOCALE_SUPPORT

/* Unicode support using libc */

void FAST_FUNC init_unicode(void)
{
	/* In unicode, this is a one character string */
	static const char unicode_0x394[] = { 0xce, 0x94, 0 };

	if (unicode_status != UNICODE_UNKNOWN)
		return;

	unicode_status = bb_mbstrlen(unicode_0x394) == 1 ? UNICODE_ON : UNICODE_OFF;
}

#else

/* Crude "locale support" which knows only C and Unicode locales */

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
		int bytes;
		unsigned c = (unsigned char) *src++;

		if (c <= 0x7f) {
			if (dest)
				*dest++ = c;
			if (c == '\0')
				break;
			n--;
			continue;
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
			return (size_t) -1L;
		c = (uint8_t)(c) >> bytes;

		while (--bytes) {
			unsigned ch = (unsigned char) *src++;
			if ((ch & 0xc0) != 0x80) {
				return (size_t) -1L;
			}
			c = (c << 6) + (ch & 0x3f);
		}

		/* TODO */
		/* Need to check that c isn't produced by overlong encoding */
		/* Example: 11000000 10000000 converts to NUL */
		/* 11110000 10000000 10000100 10000000 converts to 0x100 */
		/* correct encoding: 11000100 10000000 */
		if (c <= 0x7f) { /* crude check */
			return (size_t) -1L;
			//or maybe: c = 0xfffd; /* replacement character */
		}

		if (dest)
			*dest++ = c;
		n--;
	}

	return org_n - n;
}

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

#endif
