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
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include "libbb.h"

#define READ_BUF_SIZE	50


/* For Erik's nifty devps device driver */
#ifdef CONFIG_FEATURE_USE_DEVPS_PATCH
#include <linux/devps.h> 

/* find_pid_by_name()
 *  
 *  This finds the pid of the specified process,
 *  by using the /dev/ps device driver.
 *
 *  Returns a list of all matching PIDs
 */
extern long* find_pid_by_name( const char* pidName)
{
	int fd, i, j;
	char device[] = "/dev/ps";
	pid_t num_pids;
	pid_t* pid_array = NULL;
	long* pidList=NULL;

	/* open device */ 
	fd = open(device, O_RDONLY);
	if (fd < 0)
		perror_msg_and_die("open failed for `%s'", device);

	/* Find out how many processes there are */
	if (ioctl (fd, DEVPS_GET_NUM_PIDS, &num_pids)<0) 
		perror_msg_and_die("\nDEVPS_GET_PID_LIST");
	
	/* Allocate some memory -- grab a few extras just in case 
	 * some new processes start up while we wait. The kernel will
	 * just ignore any extras if we give it too many, and will trunc.
	 * the list if we give it too few.  */
	pid_array = (pid_t*) xcalloc( num_pids+10, sizeof(pid_t));
	pid_array[0] = num_pids+10;

	/* Now grab the pid list */
	if (ioctl (fd, DEVPS_GET_PID_LIST, pid_array)<0) 
		perror_msg_and_die("\nDEVPS_GET_PID_LIST");

	/* Now search for a match */
	for (i=1, j=0; i<pid_array[0] ; i++) {
		char* p;
		struct pid_info info;

	    info.pid = pid_array[i];
	    if (ioctl (fd, DEVPS_GET_PID_INFO, &info)<0)
			perror_msg_and_die("\nDEVPS_GET_PID_INFO");

		/* Make sure we only match on the process name */
		p=info.command_line+1;
		while ((*p != 0) && !isspace(*(p)) && (*(p-1) != '\\')) { 
			(p)++;
		}
		if (isspace(*(p)))
				*p='\0';

		if ((strstr(info.command_line, pidName) != NULL)
				&& (strlen(pidName) == strlen(info.command_line))) {
			pidList=xrealloc( pidList, sizeof(long) * (j+2));
			pidList[j++]=info.pid;
		}
	}
	if (pidList) {
		pidList[j]=0;
	} else {
		pidList=xrealloc( pidList, sizeof(long));
		pidList[0]=-1;
	}

	/* Free memory */
	free( pid_array);

	/* close device */
	if (close (fd) != 0) 
		perror_msg_and_die("close failed for `%s'", device);

	return pidList;
}

#else		/* CONFIG_FEATURE_USE_DEVPS_PATCH */

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
	while ((p = procps_scan(0)) != 0) {
		if (strcmp(p->short_cmd, pidName) == 0) {
			pidList=xrealloc( pidList, sizeof(long) * (i+2));
			pidList[i++]=p->pid;
		}
	}

	pidList[i] = i==0 ? -1 : 0;
	return pidList;
}
#endif							/* CONFIG_FEATURE_USE_DEVPS_PATCH */

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
