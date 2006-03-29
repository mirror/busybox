/* vi: set sw=4 ts=4: */
/*
 * *printf implementations for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* Mar 12, 2003     Manuel Novoa III
 *
 * While fwrite(), fputc(), fputs(), etc. all set the stream error flag
 * on failure, the *printf functions are unique in that they can fail
 * for reasons not related to the actual output itself.  Among the possible
 * reasons for failure which don't set the streams error indicator,
 * SUSv3 lists EILSEQ, EINVAL, and ENOMEM.
 *
 * In some cases, it would be desirable to have a group of *printf()
 * functions available that _always_ set the stream error indicator on
 * failure.  That would allow us to defer error checking until applet
 * exit.  Unfortunately, there is no standard way of setting a streams
 * error indicator... even though we can clear it with clearerr().
 */

/* Mar 22, 2006     Rich Felker III
 *
 * Actually there is a portable way to set the error indicator. See below.
 * It is not thread-safe as written due to a race condition with file
 * descriptors but since BB is not threaded that does not matter. It can be
 * made thread-safe at the expense of slightly more code, if this is ever
 * needed in the future.
 */

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include "libbb.h"

#ifdef L_bb_vfprintf
int bb_vfprintf(FILE * __restrict stream,
					   const char * __restrict format,
					   va_list arg)
{
	int rv;

	if ((rv = vfprintf(stream, format, arg)) < 0) {
		/* The following sequence portably sets the error flag for
		 * stream on any remotely POSIX-compliant implementation. */

		int errno_save = errno;
		int fd = fileno(stream);
		int tmp = dup(fd);

		fflush(stream);
		close(fd);
		/* Force an attempted write to nonexistant fd => EBADF */
		fputc(0, stream);
		fflush(stream);
		/* Restore the stream's original fd */
		dup2(tmp, fd);
		close(tmp);
		errno = errno_save;
	}

	return rv;
}
#endif

#ifdef L_bb_vprintf
int bb_vprintf(const char * __restrict format, va_list arg)
{
	return bb_vfprintf(stdout, format, arg);
}
#endif

#ifdef L_bb_fprintf
int bb_fprintf(FILE * __restrict stream,
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
int bb_printf(const char * __restrict format, ...)
{
	va_list arg;
	int rv;

	va_start(arg, format);
	rv = bb_vfprintf(stdout, format, arg);
	va_end(arg);

	return rv;
}
#endif
