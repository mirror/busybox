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


static int uid=-1;
static int gid=0;
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



static int fileAction(const char *fileName, struct stat* statbuf)
{
    switch (whichApp) {
	case CHGRP_APP:
	case CHOWN_APP:
	    if (chown(fileName, ((whichApp==CHOWN_APP)? uid: statbuf->st_uid), gid) < 0)
		return( TRUE);
	case CHMOD_APP:
	    fprintf(stderr, "%s, %d\n", fileName, mode);
	    if (chmod(fileName, mode))
		return( TRUE);
    }
    perror(fileName);
    return( FALSE);
}

int chmod_chown_chgrp_main(int argc, char **argv)
{
    struct group *grp;
    struct passwd *pwd;
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
	//mode &= andWithMode;
	fprintf(stderr, "mode %d\n", mode);
    } else {

	/* Find the selected group */
	groupName = strchr(*argv, '.');
	if ( whichApp==TRUE && groupName )
	    *groupName++ = '\0';
	else
	    groupName = *argv;
	grp = getgrnam(groupName);
	if (grp == NULL) {
	    fprintf(stderr, "%s: Unknown group name: %s\n", invocationName, groupName);
	    exit( FALSE);
	}
	gid = grp->gr_gid;

	/* Find the selected user (if appropriate)  */
	if (whichApp==TRUE) {
	    pwd = getpwnam(*argv);
	    if (pwd == NULL) {
		fprintf(stderr, "%s: Unknown user name: %s\n", invocationName, *argv);
		exit( FALSE);
	    }
	    uid = pwd->pw_uid;
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
}












#ifdef fooo















#include "internal.h"
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <stdio.h>

int my_getid(const char *filename, const char *name, uid_t *id) 
{
	FILE *stream;
	uid_t rid;
	char *rname, *start, *end, buf[128];

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
			if (0 == strcmp(rname, name)) {
				*id=rid;
				return 0;
			}
		} else {
			if ( *id == rid )
				return 0;
		}
	}
	fclose(stream);
	return -1;
}

int 
my_getpwuid(uid_t *uid) 
{
	return my_getid("/etc/passwd", NULL, uid);
}

int 
my_getpwnam(char *name, uid_t *uid) 
{
	return my_getid("/etc/passwd", name, uid);
}

int 
my_getgrgid(gid_t *gid) 
{
	return my_getid("/etc/group", NULL, gid);
}

int 
my_getgrnam(char *name, gid_t *gid) 
{
	return my_getid("/etc/group", name, gid);
}

const char	chown_usage[] = "chown [-R] user-name file [file ...]\n"
"\n\tThe group list is kept in the file /etc/groups.\n\n"
"\t-R:\tRecursively change the mode of all files and directories\n"
"\t\tunder the argument directory.";

int
parse_user_name(const char * s, struct FileInfo * i)
{
	char *				dot = strchr(s, '.');
	char * end = NULL;
	uid_t id = 0;

	if (! dot )
		dot = strchr(s, ':');

	if ( dot )
		*dot = '\0';

	if ( my_getpwnam(s,&id) == -1 ) {
		id = strtol(s,&end,10);
		if ((*end != '\0') || ( my_getpwuid(&id) == -1 )) {
			fprintf(stderr, "%s: no such user.\n", s);
			return 1;
		}
	}
	i->userID = id;

	if ( dot ) {
		if ( my_getgrnam(++dot,&id) == -1 ) {
			id = strtol(dot,&end,10);
			if ((*end != '\0') || ( my_getgrgid(&id) == -1 )) {
				fprintf(stderr, "%s: no such group.\n", dot);
				return 1;
			}
		}
		i->groupID = id;
		i->changeGroupID = 1;
	}
	return 0;
}

extern int
chown_main(struct FileInfo * i, int argc, char * * argv)
{
	int					status;

	while ( argc >= 3 && strcmp("-R", argv[1]) == 0 ) {
		i->recursive = 1;
		argc--;
		argv++;
	}

	if ( (status = parse_user_name(argv[1], i)) != 0 )
		return status;

	argv++;
	argc--;

	i->changeUserID = 1;
	i->complainInPostProcess = 1;

	return monadic_main(i, argc, argv);
}




#endif
