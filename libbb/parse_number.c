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
#include <string.h>
#include <stdlib.h>
#include "libbb.h"


unsigned long parse_number(const char *numstr,
		const struct suffix_mult *suffixes)
{
	const struct suffix_mult *sm;
	unsigned long int ret;
	int len;
	char *end;
	
	ret = strtoul(numstr, &end, 10);
	if (numstr == end)
		error_msg_and_die("invalid number `%s'", numstr);
	while (end[0] != '\0') {
		sm = suffixes;
		while ( sm != 0 ) {
			if(sm->suffix) {
				len = strlen(sm->suffix);
				if (strncmp(sm->suffix, end, len) == 0) {
					ret *= sm->mult;
					end += len;
					break;
				}
			sm++;
			
			} else
				sm = 0;
		}
		if (sm == 0)
			error_msg_and_die("invalid number `%s'", numstr);
	}
	return ret;
}


/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
