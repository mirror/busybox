/* vi: set sw=4 ts=4: */
/*
 * bb_get_last_path_component implementation for busybox
 *
 * Copyright (C) 2001  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

char *bb_get_last_path_component(char *path)
{
	char *first = path;
	char *last;

	last = path - 1;

	while (*path) {
		if ((*path != '/') && (path > ++last)) {
			last = first = path;
		}
		++path;
	}

	if (*first == '/') {
		last = first;
	}
	last[1] = 0;

	return first;
}
