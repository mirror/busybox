/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) tons of folks.  Tracking down who wrote what
 * isn't something I'm going to worry about...  If you wrote something
 * here, please feel free to acknowledge your work.
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
 * Based in part on code from sash, Copyright (c) 1999 by David I. Bell 
 * Permission has been granted to redistribute this code under the GPL.
 *
 */

#include <stdio.h>
#include "libbb.h"


/*
 * Routine to see if a text string is matched by a wildcard pattern.
 * Returns TRUE if the text is matched, or FALSE if it is not matched
 * or if the pattern is invalid.
 *  *		matches zero or more characters
 *  ?		matches a single character
 *  [abc]	matches 'a', 'b' or 'c'
 *  \c		quotes character c
 * Adapted from code written by Ingo Wilken, and
 * then taken from sash, Copyright (c) 1999 by David I. Bell
 * Permission is granted to use, distribute, or modify this source,
 * provided that this copyright notice remains intact.
 * Permission to distribute this code under the GPL has been granted.
 */
extern int check_wildcard_match(const char *text, const char *pattern)
{
	const char *retryPat;
	const char *retryText;
	int ch;
	int found;
	int len;

	retryPat = NULL;
	retryText = NULL;

	while (*text || *pattern) {
		ch = *pattern++;

		switch (ch) {
		case '*':
			retryPat = pattern;
			retryText = text;
			break;

		case '[':
			found = FALSE;

			while ((ch = *pattern++) != ']') {
				if (ch == '\\')
					ch = *pattern++;

				if (ch == '\0')
					return FALSE;

				if (*text == ch)
					found = TRUE;
			}
			len=strlen(text);
			if (found == FALSE && len!=0) {
				return FALSE;
			}
			if (found == TRUE) {
				if (strlen(pattern)==0 && len==1) {
					return TRUE;
				}
				if (len!=0) {
					text++;
					continue;
				}
			}

			/* fall into next case */

		case '?':
			if (*text++ == '\0')
				return FALSE;

			break;

		case '\\':
			ch = *pattern++;

			if (ch == '\0')
				return FALSE;

			/* fall into next case */

		default:
			if (*text == ch) {
				if (*text)
					text++;
				break;
			}

			if (*text) {
				pattern = retryPat;
				text = ++retryText;
				break;
			}

			return FALSE;
		}

		if (pattern == NULL)
			return FALSE;
	}

	return TRUE;
}


/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
