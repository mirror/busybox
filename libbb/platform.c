/*
 * Replacements for common but usually nonstandard functions that aren't
 * supplied by all platforms.
 *
 * Copyright (C) 2009 by Dan Fandrich <dan@coneharvesters.com>, et. al.
 *
 * Licensed under the GPL version 2, see the file LICENSE in this tarball.
 */

#include "libbb.h"

#ifndef HAVE_STRCHRNUL
char * FAST_FUNC strchrnul(const char *s, int c)
{
	while (*s && *s != c) ++s;
	return (char*)s;
}
#endif

#ifndef HAVE_VASPRINTF
int FAST_FUNC vasprintf(char **string_ptr, const char *format, va_list p)
{
	int r;
	va_list p2;

	va_copy(p2, p);
	r = vsnprintf(NULL, 0, format, p);
	va_end(p);
	*string_ptr = xmalloc(r+1);
	if (!*string_ptr)
		r = -1;
	else
		r = vsnprintf(*string_ptr, r+1, format, p2);
	va_end(p2);

	return r;
}
#endif

#ifndef HAVE_FDPRINTF
int fdprintf(int fd, const char *format, ...)
{
	va_list p;
	int r;
	char *string_ptr;

	va_start(p, format);
	r = vasprintf(&string_ptr, format, p);
	va_end(p);
	if (r >= 0) {
		r = full_write(fd, string_ptr, r);
		free(string_ptr);
	}
	return r;
}
#endif

