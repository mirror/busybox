/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) many different people.  If you wrote this, please
 * acknowledge your work.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include <stdio.h>
#include <stdlib.h>
#include "libbb.h"


/* This function parses the sort of string you might pass
 * to chmod (i.e., [ugoa]{+|-|=}[rwxst] ) and returns the
 * correct mode described by the string. */
extern int parse_mode(const char *s, mode_t * theMode)
{
	static const mode_t group_set[] = { 
		S_ISUID | S_IRWXU,		/* u */
		S_ISGID | S_IRWXG,		/* g */
		S_IRWXO,				/* o */
		S_ISUID | S_ISGID | S_ISVTX | S_IRWXU | S_IRWXG | S_IRWXO /* a */
	};

	static const mode_t mode_set[] = {
		S_IRUSR | S_IRGRP | S_IROTH, /* r */
		S_IWUSR | S_IWGRP | S_IWOTH, /* w */
		S_IXUSR | S_IXGRP | S_IXOTH, /* x */
		S_ISUID | S_ISGID,		/* s */
		S_ISVTX					/* t */
	};

	static const char group_chars[] = "ugoa";
	static const char mode_chars[] = "rwxst";

	const char *p;

	mode_t andMode =
		S_ISVTX | S_ISUID | S_ISGID | S_IRWXU | S_IRWXG | S_IRWXO;
	mode_t orMode = 0;
	mode_t mode;
	mode_t groups;
	char type;
	char c;

	if (s==NULL) {
		return (FALSE);
	}

	do {
		mode = 0;
		groups = 0;
	NEXT_GROUP:
		if ((c = *s++) == '\0') {
			return -1;
		}
		for (p=group_chars ; *p ; p++) {
			if (*p == c) {
				groups |= group_set[(int)(p-group_chars)];
				goto NEXT_GROUP;
			}
		}
		switch (c) {
			case '=':
			case '+':
			case '-':
				type = c;
				if (groups == 0) { /* The default is "all" */
					groups |= S_ISUID | S_ISGID | S_ISVTX
							| S_IRWXU | S_IRWXG | S_IRWXO;
				}
				break;
			default:
				if ((c < '0') || (c > '7') || (mode | groups)) {
					return (FALSE);
				} else {
					*theMode = strtol(--s, NULL, 8);
					return (TRUE);
				}
		}

	NEXT_MODE:
		if (((c = *s++) != '\0') && (c != ',')) {
			for (p=mode_chars ; *p ; p++) {
				if (*p == c) {
					mode |= mode_set[(int)(p-mode_chars)];
					goto NEXT_MODE;
				}
			}
			break;				/* We're done so break out of loop.*/
		}
		switch (type) {
			case '=':
				andMode &= ~(groups); /* Now fall through. */
			case '+':
				orMode |= mode & groups;
				break;
			case '-':
				andMode &= ~(mode & groups);
				orMode &= ~(mode & groups);
				break;
		}
	} while (c == ',');

	*theMode &= andMode;
	*theMode |= orMode;

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
