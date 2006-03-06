/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under the GPL v2, see the file LICENSE in this tarball.
 */

#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "libbb.h"

/* find_pid_by_name()
 *
 *  Modified by Vladimir Oleynik for use with libbb/procps.c
 *  This finds the pid of the specified process.
 *  Currently, it's implemented by rummaging through
 *  the proc filesystem.
 *
 *  Returns a list of all matching PIDs
 *  It is the caller's duty to free the returned pidlist.
 */
long* find_pid_by_name( const char* pidName)
{
	long* pidList;
	int i=0;
	procps_status_t * p;

	pidList = xmalloc(sizeof(long));
	while ((p = procps_scan(0)) != 0)
	{
		if (strncmp(p->short_cmd, pidName, COMM_LEN-1) == 0) {
			pidList=xrealloc( pidList, sizeof(long) * (i+2));
			pidList[i++]=p->pid;
		}
	}

	pidList[i] = i==0 ? -1 : 0;
	return pidList;
}

long *pidlist_reverse(long *pidList)
{
	int i=0;
	while (pidList[i] > 0 && ++i);
	if ( i-- > 0) {
		long k;
		int j;
		for (j = 0; i > j; i--, j++) {
			k = pidList[i];
			pidList[i] = pidList[j];
			pidList[j] = k;
		}
	}
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
