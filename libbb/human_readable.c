/* vi: set sw=4 ts=4: */
/*
 * make_human_readable_str
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
#include "libbb.h"



const char *make_human_readable_str(unsigned long size, 
		unsigned long block_size, unsigned long display_unit)
{
	static char str[10] = "\0";
	static const char strings[] = { 0, 'k', 'M', 'G', 'T', 0 };

	if(size == 0 || block_size == 0)
		return("0");
	
	if(display_unit) {
		snprintf(str, 9, "%ld", (size/display_unit)*block_size);
	} else {
		/* Ok, looks like they want us to autoscale */
		int i=0;
		unsigned long divisor = 1;
		long double result = size*block_size;
		for(i=0; i <= 4; i++) {
			divisor<<=10;
			if (result <= divisor) {
				divisor>>=10;
				break;
			}
		}
		result/=divisor;
		if (result > 10)
			snprintf(str, 9, "%.0Lf%c", result, strings[i]);
		else
			snprintf(str, 9, "%.1Lf%c", result, strings[i]);
	}
	return(str);
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
