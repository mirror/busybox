/*
 * Mini ps implementation for busybox
 *
 * Copyright (C) 1998 by Erik Andersen <andersee@debian.org>
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
 *
 */

#include "internal.h"
#include <unistd.h>
#include <dirent.h>
#include <stdio.h>


extern int ps_main(int argc, char **argv)
{
    DIR *dir;
    FILE *file;
    struct dirent *entry;

    if ( argc>1 && **(argv+1) == '-' ) {
	usage ("ps\n");
    }
    
    dir = opendir("/proc");
    if (!dir) {
	perror("Can't open /proc");
	exit(FALSE);
    }

    fprintf(stdout, "PID\tUid\tGid\tState\tName\n");
    while ((entry = readdir(dir)) != NULL) {
	char psStatus[NAME_MAX];
	char psName[NAME_MAX]="";
	char psState[NAME_MAX]="";
	int psPID=0, psPPID=0, psUid=0, psGid=0;
	//if (match(entry->d_name, "[0-9]") == FALSE)
	//    continue;
	sprintf(psStatus, "/proc/%s/status", entry->d_name);
	file = fopen( psStatus, "r");
	if (file == NULL) {
	    continue;
	    //perror(psStatus);
	    //exit( FALSE);
	}
	fscanf(file, "Name:\t%s\nState:\t%s\nPid:\t%d\nPPid:\t%d\nUid:\t%d\nGid:\t%d", 
		psName, psState, &psPID, &psPPID, &psUid, &psGid);
	fclose(file);

	fprintf(stdout, "%d\t%d\t%d\t%s\t%s\n", psPID, psUid, psGid, psState, psName);
    }
    closedir(dir);
    exit(TRUE);
}

