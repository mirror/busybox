/*
 * Mini find implementation for busybox
 *
 *
 * Copyright (C) 1999 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
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
#include "regexp.h"
#include <stdio.h>
#include <unistd.h>
#include <dirent.h>


static char* pattern=NULL;
static char* directory=".";
static int dereferenceFlag=FALSE;

static const char find_usage[] = "find [path...] [expression]\n"
"default path is the current directory; default expression is -print\n"
"expression may consist of:\n";



static int fileAction(const char *fileName, struct stat* statbuf)
{
    if (pattern==NULL)
	fprintf(stdout, "%s\n", fileName);
    else if (find_match((char*)fileName, pattern, TRUE) == TRUE)
	fprintf(stdout, "%s\n", fileName);
    return( TRUE);
}

int find_main(int argc, char **argv)
{
    /* peel off the "find" */
    argc--;
    argv++;

    if ( argc > 0 && **argv != '-') {
	directory=*argv;
	argc--;
	argv++;
    }

    /* Parse any options */
    while (argc > 0 && **argv == '-') {
	int stopit=FALSE;
	while (*++(*argv) && stopit==FALSE) switch (**argv) {
	    case 'f':
		if (strcmp(*argv, "follow")==0) {
		    argc--;
		    argv++;
		    dereferenceFlag=TRUE;
		}
		break;
	    case 'n':
		if (strcmp(*argv, "name")==0) {
		    if (argc-- > 1) {
			pattern=*(++argv);
			stopit=-TRUE;
		    } else {
			usage (find_usage);
		    }
		}
		break;
	    case '-':
		/* Ignore all long options */
		break;
	    default:
		usage (find_usage);
	}
	if (argc-- > 1)
	    argv++;
	    if (**argv != '-')
		break;
	else
	    break;
    }

    if (recursiveAction(directory, TRUE, FALSE, FALSE,
			   fileAction, fileAction) == FALSE) {
	exit( FALSE);
    }

    exit(TRUE);
}
