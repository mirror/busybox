/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Continue reading from file until the terminating string is encountered.
 * Return data as string.
 * e.g. fgets_str(file, "\n"); will read till end of file
 */

char *fgets_str(FILE *file, const char *terminating_string)
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
			break;
		}

		/* grow the line buffer as necessary */
		while (idx > linebufsz - 2) {
			linebuf = realloc(linebuf, linebufsz += 1000); /* GROWBY */
		}

		linebuf[idx] = ch;
		idx++;

		/* Check for terminating string */
		end_string_offset = idx - term_length;
		if ((end_string_offset > 0) && (memcmp(&linebuf[end_string_offset], terminating_string, term_length) == 0)) {
			idx -= term_length;
			break;
		}
	}
	if (idx == 0) {
		return NULL;
	}
	linebuf[idx] = '\0';
	return(linebuf);
}

