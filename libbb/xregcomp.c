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
#include "libbb.h"
#include <regex.h>



void xregcomp(regex_t *preg, const char *regex, int cflags)
{
	int ret;
	if ((ret = regcomp(preg, regex, cflags)) != 0) {
		int errmsgsz = regerror(ret, preg, NULL, 0);
		char *errmsg = xmalloc(errmsgsz);
		regerror(ret, preg, errmsg, errmsgsz);
		bb_error_msg_and_die("xregcomp: %s", errmsg);
	}
}


/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
