/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2001 Erik Andersen <andersee@debian.org>
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
#include <sys/stat.h>
#include "libbb.h"


extern void print_file(FILE *file)
{
	fflush(stdout);
	copyfd(fileno(file), fileno(stdout));
	fclose(file);
}

extern int print_file_by_name(char *filename)
{
	struct stat statBuf;
	int status = TRUE;

	if(is_directory(filename, TRUE, &statBuf)==TRUE) {
		error_msg("%s: Is directory", filename);
		status = FALSE;
	} else {
		FILE *f = wfopen(filename, "r");
		if(f!=NULL)
			print_file(f);
		else
			status = FALSE;
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
