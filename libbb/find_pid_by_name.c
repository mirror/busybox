/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

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
pid_t* find_pid_by_name(const char* procName)
{
	pid_t* pidList;
	int i = 0;
	procps_status_t* p = NULL;

	pidList = xmalloc(sizeof(*pidList));
	while ((p = procps_scan(p, PSSCAN_PID|PSSCAN_COMM))) {
		if (strncmp(p->comm, procName, sizeof(p->comm)-1) == 0) {
			pidList = xrealloc(pidList, sizeof(*pidList) * (i+2));
			pidList[i++] = p->pid;
		}
	}

	pidList[i] = 0;
	return pidList;
}

pid_t *pidlist_reverse(pid_t *pidList)
{
	int i = 0;
	while (pidList[i])
		i++;
	if (--i >= 0) {
		pid_t k;
		int j;
		for (j = 0; i > j; i--, j++) {
			k = pidList[i];
			pidList[i] = pidList[j];
			pidList[j] = k;
		}
	}
	return pidList;
}
