/*
 * Mini chown/chgrp implementation for busybox
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

#include <stdio.h>
#include <grp.h>
#include <pwd.h>
#include "internal.h"


static int uid=-1;
static int gid=0;
static int chownApp;
static char* invocationName=NULL;


static const char chgrp_usage[] = "[OPTION]... GROUP FILE...\n"
    "Change the group membership of each FILE to GROUP.\n"
    "\n\tOptions:\n" "\t-R\tchange files and directories recursively\n";
static const char chown_usage[] = "[OPTION]...  OWNER[.[GROUP] FILE...\n"
    "Change the owner and/or group of each FILE to OWNER and/or GROUP.\n"
    "\n\tOptions:\n" "\t-R\tchange files and directories recursively\n";



static int fileAction(const char *fileName)
{
    struct stat statBuf;
    if ((stat(fileName, &statBuf) < 0) || 
	    (chown(fileName, 
		   ((chownApp==TRUE)? uid: statBuf.st_uid), 
		   gid) < 0)) { 
	perror(fileName);
	return( TRUE);
    }
    return( FALSE);
}

int chown_main(int argc, char **argv)
{
    struct group *grp;
    struct passwd *pwd;
    int recursiveFlag=FALSE;
    char *groupName;


    chownApp = (strcmp(*argv, "chown")==0)? TRUE : FALSE;

    if (argc < 2) {
	fprintf(stderr, "Usage: %s %s", *argv, 
		(chownApp==TRUE)? chown_usage : chgrp_usage);
	return( FALSE);
    }
    invocationName=*argv;
    argc--;
    argv++;

    /* Parse options */
    while (**argv == '-') {
	while (*++(*argv)) switch (**argv) {
	    case 'R':
		recursiveFlag = TRUE;
		break;
	    default:
		fprintf(stderr, "Unknown option: %c\n", **argv);
		return( FALSE);
	}
	argc--;
	argv++;
    }
    
    /* Find the selected group */
    groupName = strchr(*argv, '.');
    if ( chownApp==TRUE && groupName )
	*groupName++ = '\0';
    else
	groupName = *argv;
    grp = getgrnam(groupName);
    if (grp == NULL) {
	fprintf(stderr, "%s: Unknown group name: %s\n", invocationName, groupName);
	return( FALSE);
    }
    gid = grp->gr_gid;

    /* Find the selected user (if appropriate)  */
    if (chownApp==TRUE) {
	pwd = getpwnam(*argv);
	if (pwd == NULL) {
	    fprintf(stderr, "%s: Unknown user name: %s\n", invocationName, *argv);
	    return( FALSE);
	}
	uid = pwd->pw_uid;
    }

    /* Ok, ready to do the deed now */
    if (argc <= 1) {
	fprintf(stderr, "%s: too few arguments", invocationName);
	return( FALSE);
    }
    while (argc-- > 1) {
	argv++;
	if (recursiveFlag==TRUE) 
	    recursiveAction( *argv, TRUE, fileAction, fileAction);
	else
	    fileAction( *argv);
    }
    return(TRUE);
}
