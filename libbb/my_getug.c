/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2004 by Tito Ragusa <farmatito@tiscali.it>
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

 /* 
  *
  * if bufsize is > 0 char *buffer can not be set to NULL.
  *                   If idname is not NULL it is written on the static allocated buffer 
  *                   (and a pointer to it is returned).
  *                   if idname is NULL, id as string is written to the static allocated buffer
  *                   and NULL is returned.
  * if bufsize is = 0 char *buffer can be set to NULL.
  *                   If idname exists a pointer to it is returned,
  *                   else NULL is returned.
  * if bufsize is < 0 char *buffer can be set to NULL.
  *                   If idname exists a pointer to it is returned,
  *                   else an error message is printed and the program exits.   
  */
  
#include <stdio.h>
#include <assert.h>
#include "libbb.h"


/* internal function for my_getpwuid and my_getgrgid */
char * my_getug(char *buffer, char *idname, long id, int bufsize, char prefix)
{
	if(bufsize > 0 ) {
		assert(buffer!=NULL);
		if(idname) {
			return safe_strncpy(buffer, idname, bufsize);
		}
		snprintf(buffer, bufsize, "%ld", id);
	} else if(bufsize < 0 && !idname) {
		bb_error_msg_and_die("unknown %cid %ld", prefix, id); 
	}
	return idname;
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
