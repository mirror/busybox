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

 /* Hacked by Tito Ragusa (c) 2004 <farmatito@tiscali.it> to make it more
  * flexible :
  *
  * if bufsize is > 0 char *name can not be set to NULL.
  *                   On success username is written on the static allocated buffer name 
  *                   (and a pointer to it is returned).
  *                   On failure uid as string is written to the static allocated buffer name
  *                   and NULL is returned.
  * if bufsize is = 0 char *name can be set to NULL.
  *                   On success username is returned. 
  *                   On failure NULL is returned.
  * if bufsize is < 0 char *name can be set to NULL
  *                   On success username is returned.
  *                   On failure an error message is printed and the program exits.   
  */
  
#include "libbb.h"
#include "pwd_.h"

/* gets a username given a uid */
char * my_getpwuid(char *name, long uid, int bufsize)
{
	struct passwd *myuser = getpwuid(uid);

	return  my_getug(name, (myuser) ? myuser->pw_name : (char *)myuser , uid, bufsize, 'u');
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
