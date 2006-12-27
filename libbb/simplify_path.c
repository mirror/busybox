/* vi: set sw=4 ts=4: */
/*
 * bb_simplify_path implementation for busybox
 *
 * Copyright (C) 2001  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

char *bb_simplify_path(const char *path)
{
	char *s, *start, *p;

	if (path[0] == '/')
		start = xstrdup(path);
	else {
		s = xgetcwd(NULL);
		start = concat_path_file(s, path);
		free(s);
	}
	p = s = start;

	do {
		if (*p == '/') {
			if (*s == '/') {	/* skip duplicate (or initial) slash */
				continue;
			} else if (*s == '.') {
				if (s[1] == '/' || s[1] == 0) {	/* remove extra '.' */
					continue;
				} else if ((s[1] == '.') && (s[2] == '/' || s[2] == 0)) {
					++s;
					if (p > start) {
						while (*--p != '/');	/* omit previous dir */
					}
					continue;
				}
			}
		}
		*++p = *s;
	} while (*++s);

	if ((p == start) || (*p != '/')) {	/* not a trailing slash */
		++p;					/* so keep last character */
	}
	*p = 0;

	return start;
}
