/* vi: set sw=4 ts=4: */
/*
 * Licensed under the GPL version 2, see the file LICENSE in this tarball.
 */
#ifndef UNICODE_H
#define UNICODE_H 1

enum {
	UNICODE_UNKNOWN = 0,
	UNICODE_OFF = 1,
	UNICODE_ON = 2,
};

#if !ENABLE_FEATURE_ASSUME_UNICODE

# define bb_mbstrlen(string) strlen(string)
# define unicode_status UNICODE_OFF
# define init_unicode() ((void)0)

#else

size_t bb_mbstrlen(const char *string) FAST_FUNC;

# if ENABLE_LOCALE_SUPPORT

#  include <wchar.h>
#  include <wctype.h>
extern uint8_t unicode_status;
void init_unicode(void) FAST_FUNC;

# else

/* Crude "locale support" which knows only C and Unicode locales */

#  if !ENABLE_FEATURE_CHECK_UNICODE_IN_ENV
#   define unicode_status UNICODE_ON
#   define init_unicode() ((void)0)
#  else
extern uint8_t unicode_status;
void init_unicode(void) FAST_FUNC;
#  endif

#  undef MB_CUR_MAX
#  define MB_CUR_MAX 6

/* Prevent name collisions */
#  define wint_t    bb_wint_t
#  define mbstate_t bb_mbstate_t
#  define mbstowcs  bb_mbstowcs
#  define wcstombs  bb_wcstombs
#  define wcrtomb   bb_wcrtomb
#  define iswspace  bb_iswspace
#  define iswalnum  bb_iswalnum
#  define iswpunct  bb_iswpunct

typedef int32_t wint_t;
typedef struct {
	char bogus;
} mbstate_t;

size_t mbstowcs(wchar_t *dest, const char *src, size_t n) FAST_FUNC;
size_t wcstombs(char *dest, const wchar_t *src, size_t n) FAST_FUNC;
size_t wcrtomb(char *s, wchar_t wc, mbstate_t *ps) FAST_FUNC;
int iswspace(wint_t wc) FAST_FUNC;
int iswalnum(wint_t wc) FAST_FUNC;
int iswpunct(wint_t wc) FAST_FUNC;

# endif /* !LOCALE_SUPPORT */

#endif /* FEATURE_ASSUME_UNICODE */

#endif
