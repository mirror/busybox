/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2003 by Erik Andersen <andersen@codepoet.org>
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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "libbb.h"

#define COMM_LEN 16 /* synchronize with size of comm in struct task_struct 
					   in /usr/include/linux/sched.h */


/* find_pid_by_name()
 *  
 *  Modified by Vladimir Oleynik for use with libbb/procps.c
 *  This finds the pid of the specified process.
 *  Currently, it's implemented by rummaging through 
 *  the proc filesystem.
 *
 *  Returns a list of all matching PIDs
 */
extern long* find_pid_by_name( const char* pidName)
{
	long* pidList;
	int i=0;
	procps_status_t * p;

	pidList = xmalloc(sizeof(long));
#ifdef CONFIG_SELINUX
	while ((p = procps_scan(0, 0, NULL)) != 0) {
#else
	while ((p = procps_scan(0)) != 0) {
#endif
		if (strncmp(p->short_cmd, pidName, COMM_LEN-1) == 0) {
			pidList=xrealloc( pidList, sizeof(long) * (i+2));
			pidList[i++]=p->pid;
		}
	}

	pidList[i] = i==0 ? -1 : 0;
	return pidList;
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
