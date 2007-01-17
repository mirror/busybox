/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2005, 2006 Rob Landley <rob@landley.net>
 * Copyright (C) 2004 Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2001 Matt Krai
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

/* This function reads an entire line from a text file, up to a newline
 * or NUL byte, inclusive.  It returns a malloc'ed char * which must be
 * stored and free'ed by the caller.  If end is NULL '\n' isn't considered
 * end of line.  If end isn't NULL, length of the chunk read is stored in it.
 * Return NULL if EOF/error */

char *bb_get_chunk_from_file(FILE * file, int *end)
{
	int ch;
	int idx = 0;
	char *linebuf = NULL;
	int linebufsz = 0;

	while ((ch = getc(file)) != EOF) {
		/* grow the line buffer as necessary */
		if (idx >= linebufsz) {
			linebuf = xrealloc(linebuf, linebufsz += 80);
		}
		linebuf[idx++] = (char) ch;
		if (!ch || (end && ch == '\n'))
			break;
	}
	if (end)
		*end = idx;
	if (linebuf) {
		// huh, does fgets discard prior data on error like this?
		// I don't think so....
		//if (ferror(file)) {
		//	free(linebuf);
		//	return NULL;
		//}
		linebuf = xrealloc(linebuf, idx+1);
		linebuf[idx] = '\0';
	}
	return linebuf;
}

/* Get line, including trailing \n if any */
char *xmalloc_fgets(FILE * file)
{
	int i;

	return bb_get_chunk_from_file(file, &i);
}

/* Get line.  Remove trailing \n */
char *xmalloc_getline(FILE * file)
{
	int i;
	char *c = bb_get_chunk_from_file(file, &i);

	if (i && c[--i] == '\n')
		c[i] = '\0';

	return c;
}
