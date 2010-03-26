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

#define ERROR_WCHAR (~(wchar_t)0)

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
	if (bytes == 1) {
		/* A bare "continuation" byte. Say, 80 */
		*res = ERROR_WCHAR;
		return src;
	}
	c = (uint8_t)(c) >> bytes;

	while (--bytes) {
		unsigned ch = (unsigned char) *src;
		if ((ch & 0xc0) != 0x80) {
			/* Missing "continuation" byte. Example: e0 80 */
			*res = ERROR_WCHAR;
			return src;
		}
		c = (c << 6) + (ch & 0x3f);
		src++;
	}

	/* TODO */
	/* Need to check that c isn't produced by overlong encoding */
	/* Example: 11000000 10000000 converts to NUL */
	/* 11110000 10000000 10000100 10000000 converts to 0x100 */
	/* correct encoding: 11000100 10000000 */
	if (c <= 0x7f) { /* crude check */
		*res = ERROR_WCHAR;
		return src;
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
		if (wc == ERROR_WCHAR) /* error */
			return (size_t) -1L;
		if (dest)
			*dest++ = wc;
		if (wc == 0) /* end-of-string */
			break;
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

#include "unicode_wcwidth.c"

# if ENABLE_UNICODE_BIDI_SUPPORT
int FAST_FUNC unicode_bidi_isrtl(wint_t wc)
{
	/* ranges taken from
	 * http://www.unicode.org/Public/5.2.0/ucd/extracted/DerivedBidiClass.txt
	 * Bidi_Class=Left_To_Right | Bidi_Class=Arabic_Letter
	 */
	static const struct interval rtl_b[] = {
#  define BIG_(a,b) { a, b },
#  define PAIR(a,b)
#  define ARRAY \
		PAIR(0x0590, 0x0590) \
		PAIR(0x05BE, 0x05BE) \
		PAIR(0x05C0, 0x05C0) \
		PAIR(0x05C3, 0x05C3) \
		PAIR(0x05C6, 0x05C6) \
		BIG_(0x05C8, 0x05FF) \
		PAIR(0x0604, 0x0605) \
		PAIR(0x0608, 0x0608) \
		PAIR(0x060B, 0x060B) \
		PAIR(0x060D, 0x060D) \
		BIG_(0x061B, 0x064A) \
		PAIR(0x065F, 0x065F) \
		PAIR(0x066D, 0x066F) \
		BIG_(0x0671, 0x06D5) \
		PAIR(0x06E5, 0x06E6) \
		PAIR(0x06EE, 0x06EF) \
		BIG_(0x06FA, 0x070E) \
		PAIR(0x0710, 0x0710) \
		BIG_(0x0712, 0x072F) \
		BIG_(0x074B, 0x07A5) \
		BIG_(0x07B1, 0x07EA) \
		PAIR(0x07F4, 0x07F5) \
		BIG_(0x07FA, 0x0815) \
		PAIR(0x081A, 0x081A) \
		PAIR(0x0824, 0x0824) \
		PAIR(0x0828, 0x0828) \
		BIG_(0x082E, 0x08FF) \
		PAIR(0x200F, 0x200F) \
		PAIR(0x202B, 0x202B) \
		PAIR(0x202E, 0x202E) \
		BIG_(0xFB1D, 0xFB1D) \
		BIG_(0xFB1F, 0xFB28) \
		BIG_(0xFB2A, 0xFD3D) \
		BIG_(0xFD40, 0xFDCF) \
		BIG_(0xFDC8, 0xFDCF) \
		BIG_(0xFDF0, 0xFDFC) \
		BIG_(0xFDFE, 0xFDFF) \
		BIG_(0xFE70, 0xFEFE)
		/* Probably not necessary
		{0x10800, 0x1091E},
		{0x10920, 0x10A00},
		{0x10A04, 0x10A04},
		{0x10A07, 0x10A0B},
		{0x10A10, 0x10A37},
		{0x10A3B, 0x10A3E},
		{0x10A40, 0x10A7F},
		{0x10B36, 0x10B38},
		{0x10B40, 0x10E5F},
		{0x10E7F, 0x10FFF},
		{0x1E800, 0x1EFFF}
		*/
		ARRAY
#  undef BIG_
#  undef PAIR
	};
#  define BIG_(a,b)
#  define PAIR(a,b) (a << 2) | (b-a),
	static const uint16_t rtl_p[] = { ARRAY };
#  undef BIG_
#  undef PAIR
#  define BIG_(a,b) char big_##a[b < 0x4000 && b-a <= 3 ? -1 : 1];
#  define PAIR(a,b) char pair##a[b >= 0x4000 || b-a > 3 ? -1 : 1];
	struct CHECK { ARRAY };
#  undef BIG_
#  undef PAIR
#  undef ARRAY

	if (in_interval_table(wc, rtl_b, ARRAY_SIZE(rtl_b) - 1))
		return 1;
	if (in_uint16_table(wc, rtl_p, ARRAY_SIZE(rtl_p) - 1))
		return 1;
	return 0;
}

#  if ENABLE_UNICODE_NEUTRAL_TABLE
int FAST_FUNC unicode_bidi_is_neutral_wchar(wint_t wc)
{
	/* ranges taken from
	 * http://www.unicode.org/Public/5.2.0/ucd/extracted/DerivedBidiClass.txt
	 * Bidi_Classes: Paragraph_Separator, Segment_Separator,
	 * White_Space, Other_Neutral, European_Number, European_Separator,
	 * European_Terminator, Arabic_Number, Common_Separator
	 */
	static const struct interval neutral_b[] = {
#  define BIG_(a,b) { a, b },
#  define PAIR(a,b)
#  define ARRAY \
		BIG_(0x0009, 0x000D) \
		BIG_(0x001C, 0x0040) \
		BIG_(0x005B, 0x0060) \
		PAIR(0x007B, 0x007E) \
		PAIR(0x0085, 0x0085) \
		BIG_(0x00A0, 0x00A9) \
		PAIR(0x00AB, 0x00AC) \
		BIG_(0x00AE, 0x00B4) \
		PAIR(0x00B6, 0x00B9) \
		BIG_(0x00BB, 0x00BF) \
		PAIR(0x00D7, 0x00D7) \
		PAIR(0x00F7, 0x00F7) \
		PAIR(0x02B9, 0x02BA) \
		BIG_(0x02C2, 0x02CF) \
		BIG_(0x02D2, 0x02DF) \
		BIG_(0x02E5, 0x02FF) \
		PAIR(0x0374, 0x0375) \
		PAIR(0x037E, 0x037E) \
		PAIR(0x0384, 0x0385) \
		PAIR(0x0387, 0x0387) \
		PAIR(0x03F6, 0x03F6) \
		PAIR(0x058A, 0x058A) \
		PAIR(0x0600, 0x0603) \
		PAIR(0x0606, 0x0607) \
		PAIR(0x0609, 0x060A) \
		PAIR(0x060C, 0x060C) \
		PAIR(0x060E, 0x060F) \
		BIG_(0x0660, 0x066C) \
		PAIR(0x06DD, 0x06DD) \
		PAIR(0x06E9, 0x06E9) \
		BIG_(0x06F0, 0x06F9) \
		PAIR(0x07F6, 0x07F9) \
		PAIR(0x09F2, 0x09F3) \
		PAIR(0x09FB, 0x09FB) \
		PAIR(0x0AF1, 0x0AF1) \
		BIG_(0x0BF3, 0x0BFA) \
		BIG_(0x0C78, 0x0C7E) \
		PAIR(0x0CF1, 0x0CF2) \
		PAIR(0x0E3F, 0x0E3F) \
		PAIR(0x0F3A, 0x0F3D) \
		BIG_(0x1390, 0x1400) \
		PAIR(0x1680, 0x1680) \
		PAIR(0x169B, 0x169C) \
		PAIR(0x17DB, 0x17DB) \
		BIG_(0x17F0, 0x17F9) \
		BIG_(0x1800, 0x180A) \
		PAIR(0x180E, 0x180E) \
		PAIR(0x1940, 0x1940) \
		PAIR(0x1944, 0x1945) \
		BIG_(0x19DE, 0x19FF) \
		PAIR(0x1FBD, 0x1FBD) \
		PAIR(0x1FBF, 0x1FC1) \
		PAIR(0x1FCD, 0x1FCF) \
		PAIR(0x1FDD, 0x1FDF) \
		PAIR(0x1FED, 0x1FEF) \
		PAIR(0x1FFD, 0x1FFE) \
		BIG_(0x2000, 0x200A) \
		BIG_(0x2010, 0x2029) \
		BIG_(0x202F, 0x205F) \
		PAIR(0x2070, 0x2070) \
		BIG_(0x2074, 0x207E) \
		BIG_(0x2080, 0x208E) \
		BIG_(0x20A0, 0x20B8) \
		PAIR(0x2100, 0x2101) \
		PAIR(0x2103, 0x2106) \
		PAIR(0x2108, 0x2109) \
		PAIR(0x2114, 0x2114) \
		PAIR(0x2116, 0x2118) \
		BIG_(0x211E, 0x2123) \
		PAIR(0x2125, 0x2125) \
		PAIR(0x2127, 0x2127) \
		PAIR(0x2129, 0x2129) \
		PAIR(0x212E, 0x212E) \
		PAIR(0x213A, 0x213B) \
		BIG_(0x2140, 0x2144) \
		PAIR(0x214A, 0x214D) \
		BIG_(0x2150, 0x215F) \
		PAIR(0x2189, 0x2189) \
		BIG_(0x2190, 0x2335) \
		BIG_(0x237B, 0x2394) \
		BIG_(0x2396, 0x23E8) \
		BIG_(0x2400, 0x2426) \
		BIG_(0x2440, 0x244A) \
		BIG_(0x2460, 0x249B) \
		BIG_(0x24EA, 0x26AB) \
		BIG_(0x26AD, 0x26CD) \
		BIG_(0x26CF, 0x26E1) \
		PAIR(0x26E3, 0x26E3) \
		BIG_(0x26E8, 0x26FF) \
		PAIR(0x2701, 0x2704) \
		PAIR(0x2706, 0x2709) \
		BIG_(0x270C, 0x2727) \
		BIG_(0x2729, 0x274B) \
		PAIR(0x274D, 0x274D) \
		PAIR(0x274F, 0x2752) \
		BIG_(0x2756, 0x275E) \
		BIG_(0x2761, 0x2794) \
		BIG_(0x2798, 0x27AF) \
		BIG_(0x27B1, 0x27BE) \
		BIG_(0x27C0, 0x27CA) \
		PAIR(0x27CC, 0x27CC) \
		BIG_(0x27D0, 0x27FF) \
		BIG_(0x2900, 0x2B4C) \
		BIG_(0x2B50, 0x2B59) \
		BIG_(0x2CE5, 0x2CEA) \
		BIG_(0x2CF9, 0x2CFF) \
		BIG_(0x2E00, 0x2E99) \
		BIG_(0x2E9B, 0x2EF3) \
		BIG_(0x2F00, 0x2FD5) \
		BIG_(0x2FF0, 0x2FFB) \
		BIG_(0x3000, 0x3004) \
		BIG_(0x3008, 0x3020) \
		PAIR(0x3030, 0x3030) \
		PAIR(0x3036, 0x3037) \
		PAIR(0x303D, 0x303D) \
		PAIR(0x303E, 0x303F) \
		PAIR(0x309B, 0x309C) \
		PAIR(0x30A0, 0x30A0) \
		PAIR(0x30FB, 0x30FB) \
		BIG_(0x31C0, 0x31E3) \
		PAIR(0x321D, 0x321E) \
		BIG_(0x3250, 0x325F) \
		PAIR(0x327C, 0x327E) \
		BIG_(0x32B1, 0x32BF) \
		PAIR(0x32CC, 0x32CF) \
		PAIR(0x3377, 0x337A) \
		PAIR(0x33DE, 0x33DF) \
		PAIR(0x33FF, 0x33FF) \
		BIG_(0x4DC0, 0x4DFF) \
		BIG_(0xA490, 0xA4C6) \
		BIG_(0xA60D, 0xA60F) \
		BIG_(0xA673, 0xA673) \
		BIG_(0xA67E, 0xA67F) \
		BIG_(0xA700, 0xA721) \
		BIG_(0xA788, 0xA788) \
		BIG_(0xA828, 0xA82B) \
		BIG_(0xA838, 0xA839) \
		BIG_(0xA874, 0xA877) \
		BIG_(0xFB29, 0xFB29) \
		BIG_(0xFD3E, 0xFD3F) \
		BIG_(0xFDFD, 0xFDFD) \
		BIG_(0xFE10, 0xFE19) \
		BIG_(0xFE30, 0xFE52) \
		BIG_(0xFE54, 0xFE66) \
		BIG_(0xFE68, 0xFE6B) \
		BIG_(0xFF01, 0xFF20) \
		BIG_(0xFF3B, 0xFF40) \
		BIG_(0xFF5B, 0xFF65) \
		BIG_(0xFFE0, 0xFFE6) \
		BIG_(0xFFE8, 0xFFEE) \
		BIG_(0xFFF9, 0xFFFD)
		/*
		{0x10101, 0x10101},
		{0x10140, 0x1019B},
		{0x1091F, 0x1091F},
		{0x10B39, 0x10B3F},
		{0x10E60, 0x10E7E},
		{0x1D200, 0x1D241},
		{0x1D245, 0x1D245},
		{0x1D300, 0x1D356},
		{0x1D6DB, 0x1D6DB},
		{0x1D715, 0x1D715},
		{0x1D74F, 0x1D74F},
		{0x1D789, 0x1D789},
		{0x1D7C3, 0x1D7C3},
		{0x1D7CE, 0x1D7FF},
		{0x1F000, 0x1F02B},
		{0x1F030, 0x1F093},
		{0x1F100, 0x1F10A}
		*/
		ARRAY
#  undef BIG_
#  undef PAIR
	};
#  define BIG_(a,b)
#  define PAIR(a,b) (a << 2) | (b-a),
	static const uint16_t neutral_p[] = { ARRAY };
#  undef BIG_
#  undef PAIR
#  define BIG_(a,b) char big_##a[b < 0x4000 && b-a <= 3 ? -1 : 1];
#  define PAIR(a,b) char pair##a[b >= 0x4000 || b-a > 3 ? -1 : 1];
	struct CHECK { ARRAY };
#  undef BIG_
#  undef PAIR
#  undef ARRAY

	if (in_interval_table(wc, neutral_b, ARRAY_SIZE(neutral_b) - 1))
		return 1;
	if (in_uint16_table(wc, neutral_p, ARRAY_SIZE(neutral_p) - 1))
		return 1;
	return 0;
}
#  endif

# endif /* UNICODE_BIDI_SUPPORT */

#endif /* Homegrown Unicode support */


/* The rest is mostly same for libc and for "homegrown" support */

size_t FAST_FUNC unicode_strlen(const char *string)
{
	size_t width = mbstowcs(NULL, string, INT_MAX);
	if (width == (size_t)-1L)
		return strlen(string);
	return width;
}

static char* FAST_FUNC unicode_conv_to_printable2(uni_stat_t *stats, const char *src, unsigned width, int flags)
{
	char *dst;
	unsigned dst_len;
	unsigned uni_count;
	unsigned uni_width;

	if (unicode_status != UNICODE_ON) {
		char *d;
		if (flags & UNI_FLAG_PAD) {
			d = dst = xmalloc(width + 1);
			while ((int)--width >= 0) {
				unsigned char c = *src;
				if (c == '\0') {
					do
						*d++ = ' ';
					while ((int)--width >= 0);
					break;
				}
				*d++ = (c >= ' ' && c < 0x7f) ? c : '?';
				src++;
			}
			*d = '\0';
		} else {
			d = dst = xstrndup(src, width);
			while (*d) {
				unsigned char c = *d;
				if (c < ' ' || c >= 0x7f)
					*d = '?';
				d++;
			}
		}
		if (stats)
			stats->byte_count = stats->unicode_count = (d - dst);
		return dst;
	}

	dst = NULL;
	uni_count = uni_width = 0;
	dst_len = 0;
	while (1) {
		int w;
		wchar_t wc;

#if ENABLE_LOCALE_SUPPORT
		{
			mbstate_t mbst = { 0 };
			ssize_t rc = mbsrtowcs(&wc, &src, 1, &mbst);
			/* If invalid sequence is seen: -1 is returned,
			 * src points to the invalid sequence, errno = EILSEQ.
			 * Else number of wchars (excluding terminating L'\0')
			 * written to dest is returned.
			 * If len (here: 1) non-L'\0' wchars stored at dest,
			 * src points to the next char to be converted.
			 * If string is completely converted: src = NULL.
			 */
			if (rc == 0) /* end-of-string */
				break;
			if (rc < 0) { /* error */
				src++;
				goto subst;
			}
			if (!iswprint(wc))
				goto subst;
		}
#else
		src = mbstowc_internal(&wc, src);
		/* src is advanced to next mb char
		 * wc == ERROR_WCHAR: invalid sequence is seen
		 * else: wc is set
		 */
		if (wc == ERROR_WCHAR) /* error */
			goto subst;
		if (wc == 0) /* end-of-string */
			break;
#endif
		if (CONFIG_LAST_SUPPORTED_WCHAR && wc > CONFIG_LAST_SUPPORTED_WCHAR)
			goto subst;
		w = wcwidth(wc);
		if ((ENABLE_UNICODE_COMBINING_WCHARS && w < 0) /* non-printable wchar */
		 || (!ENABLE_UNICODE_COMBINING_WCHARS && w <= 0)
		 || (!ENABLE_UNICODE_WIDE_WCHARS && w > 1)
		) {
 subst:
			wc = CONFIG_SUBST_WCHAR;
			w = 1;
		}
		width -= w;
		/* Note: if width == 0, we still may add more chars,
		 * they may be zero-width or combining ones */
		if ((int)width < 0) {
			/* can't add this wc, string would become longer than width */
			width += w;
			break;
		}

		uni_count++;
		uni_width += w;
		dst = xrealloc(dst, dst_len + MB_CUR_MAX);
#if ENABLE_LOCALE_SUPPORT
		{
			mbstate_t mbst = { 0 };
			dst_len += wcrtomb(&dst[dst_len], wc, &mbst);
		}
#else
		dst_len += wcrtomb_internal(&dst[dst_len], wc);
#endif
	}

	/* Pad to remaining width */
	if (flags & UNI_FLAG_PAD) {
		dst = xrealloc(dst, dst_len + width + 1);
		uni_count += width;
		uni_width += width;
		while ((int)--width >= 0) {
			dst[dst_len++] = ' ';
		}
	}
	dst[dst_len] = '\0';
	if (stats) {
		stats->byte_count = dst_len;
		stats->unicode_count = uni_count;
		stats->unicode_width = uni_width;
	}

	return dst;
}
char* FAST_FUNC unicode_conv_to_printable(uni_stat_t *stats, const char *src)
{
	return unicode_conv_to_printable2(stats, src, INT_MAX, 0);
}
char* FAST_FUNC unicode_conv_to_printable_maxwidth(uni_stat_t *stats, const char *src, unsigned maxwidth)
{
	return unicode_conv_to_printable2(stats, src, maxwidth, 0);
}
char* FAST_FUNC unicode_conv_to_printable_fixedwidth(uni_stat_t *stats, const char *src, unsigned width)
{
	return unicode_conv_to_printable2(stats, src, width, UNI_FLAG_PAD);
}

#ifdef UNUSED
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
		if (wc == ERROR_WCHAR || wc == 0) /* error, or end-of-string */
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
#endif
