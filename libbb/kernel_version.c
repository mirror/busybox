/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
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
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/utsname.h>		/* for uname(2) */

#include "libbb.h"

/* Returns kernel version encoded as major*65536 + minor*256 + patch,
 * so, for example,  to check if the kernel is greater than 2.2.11:
 *     if (get_kernel_revision() <= 2*65536+2*256+11) { <stuff> }
 */
extern int get_kernel_revision(void)
{
	struct utsname name;
	char *s;
	int i, r;

	if (uname(&name) == -1) {
		perror_msg("cannot get system information");
		return (0);
	}

	s = name.release;
	r = 0;
	for (i=0 ; i<3 ; i++) {
		r = r * 256 + atoi(strtok(s, "."));
		s = NULL;
	}
	return r;
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
