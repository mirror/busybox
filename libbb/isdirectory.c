/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Based in part on code from sash, Copyright (c) 1999 by David I. Bell 
 * Permission has been granted to redistribute this code under the GPL.
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
#include <sys/stat.h>
#include "libbb.h"

/*
 * Return TRUE if a fileName is a directory.
 * Nonexistant files return FALSE.
 */
int is_directory(const char *fileName, const int followLinks, struct stat *statBuf)
{
	int status;
	int didMalloc = 0;

	if (statBuf == NULL) {
	    statBuf = (struct stat *)xmalloc(sizeof(struct stat));
	    ++didMalloc;
	}

	if (followLinks == TRUE)
		status = stat(fileName, statBuf);
	else
		status = lstat(fileName, statBuf);

	if (status < 0 || !(S_ISDIR(statBuf->st_mode))) {
	    status = FALSE;
	}
	else status = TRUE;

	if (didMalloc) {
	    free(statBuf);
	    statBuf = NULL;
	}
	return status;
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
