/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) many different people.  
 * If you wrote this, please acknowledge your work.
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
 */

#include <stdio.h>
#include <stdlib.h>
#include "libbb.h"

/* get_line_from_file() - This function reads an entire line from a text file,
 * up to a newline. It returns a malloc'ed char * which must be stored and
 * free'ed  by the caller.  If 'c' is nonzero, the trailing '\n' (if any)
 * is removed.  In event of a read error or EOF, NULL is returned. */

static char *private_get_line_from_file(FILE *file, int c)
{
#define GROWBY (80)		/* how large we will grow strings by */

	int ch;
	int idx = 0;
	char *linebuf = NULL;
	int linebufsz = 0;

	while ((ch = getc(file)) != EOF) {
		/* grow the line buffer as necessary */
		if (idx > linebufsz - 2) {
			linebuf = xrealloc(linebuf, linebufsz += GROWBY);
		}
		linebuf[idx++] = (char)ch;
		if (ch == '\n' || ch == '\0') {
			if (c) {
				--idx;
			}
			break;
		}
	}
	if (linebuf) {
		if (ferror(file)) {
			free(linebuf);
			return NULL;
		}
		linebuf[idx] = 0;
	}
	return linebuf;
}

extern char *bb_get_line_from_file(FILE *file)
{
	return private_get_line_from_file(file, 0);
}

extern char *bb_get_chomped_line_from_file(FILE *file)
{
	return private_get_line_from_file(file, 1);
}


/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
