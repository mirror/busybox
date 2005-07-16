/* vi: set sw=4 ts=4: */
/*
 * Mini Cat implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
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
 */

#include <stdlib.h>
#include <string.h>
#include "busybox.h"

extern int cat_main(int argc, char **argv)
{
	int status = EXIT_SUCCESS;

	if (argc == 1) {
		print_file(stdin);
		return status;
	}

	while (--argc > 0) {
		if(!(strcmp(*++argv, "-"))) {
			print_file(stdin);
		} else if (print_file_by_name(*argv) == FALSE) {
			status = EXIT_FAILURE;
		}
	}
	return status;
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
