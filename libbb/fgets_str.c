/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) many different people.
 * If you wrote this, please acknowledge your work.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

/* Read up to (and including) TERMINATING_STRING from FILE and return it.
 * Return NULL on EOF.  */

char *xmalloc_fgets_str(FILE *file, const char *terminating_string)
{
	char *linebuf = NULL;
	const int term_length = strlen(terminating_string);
	int end_string_offset;
	int linebufsz = 0;
	int idx = 0;
	int ch;

	while (1) {
		ch = fgetc(file);
		if (ch == EOF) {
			free(linebuf);
			return NULL;
		}

		/* grow the line buffer as necessary */
		while (idx > linebufsz - 2) {
			linebufsz += 200;
			linebuf = xrealloc(linebuf, linebufsz);
		}

		linebuf[idx] = ch;
		idx++;

		/* Check for terminating string */
		end_string_offset = idx - term_length;
		if (end_string_offset > 0
		 && memcmp(&linebuf[end_string_offset], terminating_string, term_length) == 0
		) {
			idx -= term_length;
			break;
		}
	}
	linebuf = xrealloc(linebuf, idx + 1);
	linebuf[idx] = '\0';
	return linebuf;
}
