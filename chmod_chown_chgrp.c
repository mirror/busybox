/*
 * Mini chown/chmod/chgrp implementation for busybox
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


static uid_t uid=-1;
static gid_t gid=-1;
static int whichApp;
static char* invocationName=NULL;
static mode_t mode=0644;


#define CHGRP_APP   1
#define CHOWN_APP   2
#define CHMOD_APP   3

static const char chgrp_usage[] = "[OPTION]... GROUP FILE...\n"
    "Change the group membership of each FILE to GROUP.\n"
    "\n\tOptions:\n" "\t-R\tchange files and directories recursively\n";
static const char chown_usage[] = "[OPTION]...  OWNER[.[GROUP] FILE...\n"
    "Change the owner and/or group of each FILE to OWNER and/or GROUP.\n"
    "\n\tOptions:\n" "\t-R\tchange files and directories recursively\n";
static const char chmod_usage[] = "[-R] MODE[,MODE]... FILE...\n"
"Each MODE is one or more of the letters ugoa, one of the symbols +-= and\n"
"one or more of the letters rwxst.\n\n"
 "\t-R\tchange files and directories recursively.\n";


uid_t my_getid(const char *filename, const char *name) 
{
	FILE *stream;
	char *rname, *start, *end, buf[128];
	uid_t rid;

	stream=fopen(filename,"r");

	while (fgets (buf, 128, stream) != NULL) {
		if (buf[0] == '#')
			continue;

		start = buf;
		end = strchr (start, ':');
		if (end == NULL)
			continue;
		*end = '\0';
		rname = start;

		start = end + 1;
		end = strchr (start, ':');
		if (end == NULL)
			continue;

		start = end + 1;
		rid = (uid_t) strtol (start, &end, 10);
		if (end == start)
			continue;

		if (name) {
		    if (0 == strcmp(rname, name))
			return( rid);
		}
	}
	fclose(stream);
	return (-1);
}

uid_t 
my_getpwnam(char *name) 
{
    return my_getid("/etc/passwd", name);
}

gid_t 
my_getgrnam(char *name) 
{
    return my_getid("/etc/group", name);
}

static int fileAction(const char *fileName, struct stat* statbuf)
{
    switch (whichApp) {
	case CHGRP_APP:
	case CHOWN_APP:
	    if (chown(fileName, (whichApp==CHOWN_APP)? uid : statbuf->st_uid, 
			(gid==-1)? statbuf->st_gid : gid) == 0) {
		return( TRUE);
	    }
	    break;
	case CHMOD_APP:
	    if (chmod(fileName, mode) == 0)
		return( TRUE);
	    break;
    }
    perror(fileName);
    return( FALSE);
}

int chmod_chown_chgrp_main(int argc, char **argv)
{
    int recursiveFlag=FALSE;
    char *groupName;

    whichApp = (strcmp(*argv, "chown")==0)? CHOWN_APP : (strcmp(*argv, "chmod")==0)? CHMOD_APP : CHGRP_APP; 

    if (argc < 2) {
	fprintf(stderr, "Usage: %s %s", *argv, 
		(whichApp==TRUE)? chown_usage : chgrp_usage);
	exit( FALSE);
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
		exit( FALSE);
	}
	argc--;
	argv++;
    }
    
    if ( whichApp == CHMOD_APP ) {
	/* Find the specified modes */
	mode &= S_ISVTX|S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO;
	if ( parse_mode(*argv, &mode) == FALSE ) {
	    fprintf(stderr, "%s: Unknown mode: %s\n", invocationName, *argv);
	    exit( FALSE);
	}
    } else {

	/* Find the selected group */
	if ( whichApp==CHGRP_APP && groupName ) {
	    groupName = *argv;
	    gid = my_getgrnam(groupName);
	    if (gid == -1)
		goto bad_group;
	} else {
	    groupName = strchr(*argv, '.');
	    if (groupName) {
		*groupName++ = '\0';
		gid = my_getgrnam(groupName);
		if (gid == -1)
		    goto bad_group;
	    } else
		gid = -1;
	}


	/* Find the selected user (if appropriate)  */
	if (whichApp==CHOWN_APP) {
	    uid = my_getpwnam(*argv);
	    if (uid == -1) {
		fprintf(stderr, "%s: Unknown user name: %s\n", invocationName, *argv);
		exit( FALSE);
	    }
	}
    }
    
    /* Ok, ready to do the deed now */
    if (argc <= 1) {
	fprintf(stderr, "%s: too few arguments", invocationName);
	exit( FALSE);
    }
    while (argc-- > 1) {
	if (recursiveAction( *(++argv), recursiveFlag, TRUE, FALSE, fileAction, fileAction)==FALSE)
	    exit( FALSE);
    }
    exit(TRUE);

bad_group:
    fprintf(stderr, "%s: Unknown group name: %s\n", invocationName, groupName);
    exit( FALSE);
}


