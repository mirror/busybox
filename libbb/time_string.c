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
#include <time.h>
#include <utime.h>
#include "libbb.h"


/*
 * Return the standard ls-like time string from a time_t
 * This is static and so is overwritten on each call.
 */
const char *time_string(time_t timeVal)
{
	time_t now;
	char *str;
	static char buf[26];

	time(&now);

	str = ctime(&timeVal);

	strcpy(buf, &str[4]);
	buf[12] = '\0';

	if ((timeVal > now) || (timeVal < now - 365 * 24 * 60 * 60L)) {
		strcpy(&buf[7], &str[20]);
		buf[11] = '\0';
	}

	return buf;
}


/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
