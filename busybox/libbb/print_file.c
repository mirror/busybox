/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
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
#include <unistd.h>
#include "libbb.h"

extern void bb_xprint_and_close_file(FILE *file)
{
	bb_xfflush_stdout();
	/* Note: Do not use STDOUT_FILENO here, as this is a lib routine
	 *       and the calling code may have reassigned stdout. */
	if (bb_copyfd_eof(fileno(file), STDOUT_FILENO) == -1) {
		/* bb_copyfd outputs any needed messages, so just die. */
		exit(bb_default_error_retval);
	}
	/* Note: Since we're reading, don't bother checking the return value
	 *       of fclose().  The only possible failure is EINTR which
	 *       should already have been taken care of. */
	fclose(file);
}

/* Returns:
 *    0      if successful
 *   -1      if 'filename' does not exist or is a directory
 *  exits with default error code if an error occurs
 */

extern int bb_xprint_file_by_name(const char *filename)
{
	FILE *f;

#if 0
	/* This check shouldn't be necessary for linux, but is left
	* here disabled just in case. */
	struct stat statBuf;

	if(is_directory(filename, TRUE, &statBuf)) {
		bb_error_msg("%s: Is directory", filename);
	} else
#endif
	if ((f = bb_wfopen(filename, "r")) != NULL) {
		bb_xprint_and_close_file(f);
		return 0;
	}

	return -1;
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
