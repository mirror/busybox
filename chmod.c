/*
 * Mini chmod implementation for busybox
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


static mode_t mode=7777;


static const char chmod_usage[] = "[-R] MODE[,MODE]... FILE...\n"
"Each MODE is one or more of the letters ugoa, one of the symbols +-= and\n"
"one or more of the letters rwxst.\n\n"
 "\t-R\tchange files and directories recursively.\n";



static int fileAction(const char *fileName)
{
    struct stat statBuf;
    if ((stat(fileName, &statBuf) < 0) || 
	    (chmod(fileName, mode)) < 0) { 
	perror(fileName);
	return( FALSE);
    }
    return( TRUE);
}

/* [ugoa]{+|-|=}[rwxstl] */
int parse_mode( const char* s, mode_t* or, mode_t* and, int* group_execute) 
{
	mode_t	mode = 0;
	mode_t	groups = S_ISVTX;
	char	type;
	char	c;

	do {
		for ( ; ; ) {
			switch ( c = *s++ ) {
			case '\0':
				return (FALSE);
			case 'u':
				groups |= S_ISUID|S_IRWXU;
				continue;
			case 'g':
				groups |= S_ISGID|S_IRWXG;
				continue;
			case 'o':
				groups |= S_IRWXO;
				continue;
			case 'a':
				groups |= S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO;
				continue;
			case '+':
			case '=':
			case '-':
				type = c;
				if ( groups == S_ISVTX ) /* The default is "all" */
					groups |= S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO;
				break;
			default:
				if ( c >= '0' && c <= '7' && mode == 0 && groups == S_ISVTX ) {
					*and = 0;
					*or = strtol(--s, 0, 010);
					return (TRUE);
				}
				else
					return (FALSE);
			}
			break;
		}

		while ( (c = *s++) != '\0' ) {
			switch ( c ) {
			case ',':
				break;
			case 'r':
				mode |= S_IRUSR|S_IRGRP|S_IROTH;
				continue;
			case 'w':
				mode |= S_IWUSR|S_IWGRP|S_IWOTH;
				continue;
			case 'x':
				mode |= S_IXUSR|S_IXGRP|S_IXOTH;
				continue;
			case 's':
				if ( group_execute != 0 && (groups & S_IRWXG) ) {
					if ( *group_execute < 0 )
						return (FALSE);
					if ( type != '-' ) {
						mode |= S_IXGRP;
						*group_execute = 1;
					}
				}
				mode |= S_ISUID|S_ISGID;
				continue;
			case 'l':
				if ( *group_execute > 0 )
					return (FALSE);
				if ( type != '-' ) {
					*and &= ~S_IXGRP;
					*group_execute = -1;
				}
				mode |= S_ISGID;
				groups |= S_ISGID;
				continue;
			case 't':
				mode |= S_ISVTX;
				continue;
			default:
				return (FALSE);
			}
			break;
		}
		switch ( type ) {
		case '=':
			*and &= ~(groups);
			/* fall through */
		case '+':
			*or |= mode & groups;
			break;
		case '-':
			*and &= ~(mode & groups);
			*or &= *and;
			break;
		}
	} while ( c == ',' );
	return (TRUE);
}


int chmod_main(int argc, char **argv)
{
    int recursiveFlag=FALSE;
    mode_t andWithMode = S_ISVTX|S_ISUID|S_ISGID|S_IRWXU|S_IRWXG|S_IRWXO;
    mode_t orWithMode = 0;
    char *invocationName=*argv;

    if (argc < 3) {
	fprintf(stderr, "Usage: %s %s", invocationName, chmod_usage);
	exit( FALSE);
    }
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
    
    /* Find the selected group */
    if ( parse_mode(*argv, &orWithMode, &andWithMode, 0) == FALSE ) {
	fprintf(stderr, "%s: Unknown mode: %s\n", invocationName, *argv);
	exit( FALSE);
    }
    mode &= andWithMode;
    mode |= orWithMode;

    /* Ok, ready to do the deed now */
    if (argc <= 1) {
	fprintf(stderr, "%s: too few arguments", invocationName);
	exit( FALSE);
    }
    while (argc-- > 1) {
	argv++;
	recursiveAction( *argv, recursiveFlag, TRUE, fileAction, fileAction);
    }
    exit(TRUE);
}
