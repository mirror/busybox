/* vi: set sw=4 ts=4: */
/*
 * *printf implementations for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
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

/* Mar 12, 2003     Manuel Novoa III
 *
 * While fwrite(), fputc(), fputs(), etc. all set the stream error flag
 * on failure, the *printf functions are unique in that they can fail
 * for reasons not related to the actual output itself.  Among the possible
 * reasons for failure which don't set the streams error indicator,
 * SUSv3 lists EILSEQ, EINVAL, and ENOMEM.
 *
 * In some cases, it would be desireable to have a group of *printf()
 * functions available that _always_ set the stream error indicator on
 * failure.  That would allow us to defer error checking until applet
 * exit.  Unfortunately, there is no standard way of setting a streams
 * error indicator... even though we can clear it with clearerr().
 *
 * Therefore, we have to resort to implementation dependent code.  Feel
 * free to send patches for stdio implementations where the following
 * fails.
 *
 * NOTE: None of this is threadsafe.  As busybox is a nonthreaded app,
 *       that isn't currently an issue.
 */

#include <stdio.h>
#include <stdarg.h>
#include "libbb.h"

#if defined(__UCLIBC__) 

# if defined(__FLAG_ERROR)
/* Using my newer stdio implementation.  Unlocked macros are:
 * #define __CLEARERR(stream) \
	  ((stream)->modeflags &= ~(__FLAG_EOF|__FLAG_ERROR), (void)0)
 * #define __FEOF(stream)		((stream)->modeflags & __FLAG_EOF)
 * #define __FERROR(stream)	((stream)->modeflags & __FLAG_ERROR)
 */
#define SET_FERROR_UNLOCKED(S)    ((S)->modeflags |= __FLAG_ERROR)

#elif defined(__MODE_ERR)
/* Using either the original stdio implementation (from dev86) or
 * my original stdio rewrite.  Macros were:
 * #define ferror(fp)	(((fp)->mode&__MODE_ERR) != 0)
 * #define feof(fp)   	(((fp)->mode&__MODE_EOF) != 0)
 * #define clearerr(fp)	((fp)->mode &= ~(__MODE_EOF|__MODE_ERR),0)
 */
#define SET_FERROR_UNLOCKED(S)    ((S)->mode |= __MODE_ERR)

#else
#error unknown uClibc stdio implemenation!
#endif

#elif defined(__GLIBC__)

# if defined(_STDIO_USES_IOSTREAM)
/* Apparently using the newer libio implementation, with associated defines:
 * #define _IO_feof_unlocked(__fp) (((__fp)->_flags & _IO_EOF_SEEN) != 0)
 * #define _IO_ferror_unlocked(__fp) (((__fp)->_flags & _IO_ERR_SEEN) != 0)
 */
#define SET_FERROR_UNLOCKED(S)    ((S)->_flags |= _IO_ERR_SEEN)

# else
/* Assume the older version of glibc which used a bitfield entry
 * as a stream error flag.  The associated defines were:
 * #define __clearerr(stream)      ((stream)->__error = (stream)->__eof = 0)
 * #define feof_unlocked(stream)         ((stream)->__eof != 0)
 * #define ferror_unlocked(stream)       ((stream)->__error != 0)
 */
#define SET_FERROR_UNLOCKED(S)    ((S)->__error = 1)

# endif

#elif defined(__NEWLIB_H__)
/* I honestly don't know if there are different versions of stdio in
 * newlibs history.  Anyway, here's what's current.
 * #define __sfeof(p)      (((p)->_flags & __SEOF) != 0)
 * #define __sferror(p)    (((p)->_flags & __SERR) != 0)
 * #define __sclearerr(p)  ((void)((p)->_flags &= ~(__SERR|__SEOF)))
 */
#define SET_FERROR_UNLOCKED(S)    ((S)->_flags |= __SERR)

#elif defined(__dietlibc__)
/*
 *     WARNING!!!  dietlibc is quite buggy.  WARNING!!!
 *
 * Some example bugs as of March 12, 2003...
 *   1) fputc() doesn't set the error indicator on failure.
 *   2) freopen() doesn't maintain the same stream object, contary to
 *      standards.  This makes it useless in its primary role of
 *      reassociating stdin/stdout/stderr.
 *   3) printf() often fails to correctly format output when conversions
 *      involve padding.  It is also practically useless for floating
 *      point output.
 *
 * But, if you're determined to use it anyway, (as of the current version)
 * you can extract the information you need from dietstdio.h.  See the
 * other library implementations for examples.
 */
#error dietlibc is currently not supported.  Please see the commented source.

#else /* some other lib */
/* Please see the comments for the above supported libaries for examples
 * of what is required to support your stdio implementation.
 */
#error Your stdio library is currently not supported.  Please see the commented source.
#endif

#ifdef L_bb_vfprintf
extern int bb_vfprintf(FILE * __restrict stream,
					   const char * __restrict format,
					   va_list arg)
{
	int rv;

	if ((rv = vfprintf(stream, format, arg)) < 0) {
		SET_FERROR_UNLOCKED(stream);
	}

	return rv;
}
#endif

#ifdef L_bb_vprintf
extern int bb_vprintf(const char * __restrict format, va_list arg)
{
	return bb_vfprintf(stdout, format, arg);
}
#endif

#ifdef L_bb_fprintf
extern int bb_fprintf(FILE * __restrict stream,
					  const char * __restrict format, ...)
{
	va_list arg;
	int rv;

	va_start(arg, format);
	rv = bb_vfprintf(stream, format, arg);
	va_end(arg);

	return rv;
}
#endif

#ifdef L_bb_printf
extern int bb_printf(const char * __restrict format, ...)
{
	va_list arg;
	int rv;

	va_start(arg, format);
	rv = bb_vfprintf(stdout, format, arg);
	va_end(arg);

	return rv;
}
#endif
