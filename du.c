/*
 * Mini du implementation for busybox
 *
 *
 * Copyright (C) 1999 by Lineo, inc.
 * Written by John Beppu <beppu@lineo.com>
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
#include <sys/types.h>
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <errno.h>
#if 0
#include <unistd.h>
#include <sys/stat.h>
#endif

typedef void (Display)(size_t, char *);

static const char du_usage[] =
"Usage: du [OPTION]... [FILE]...\n\n"
"\t-s\tdisplay only a total for each argument\n"
;

static int	du_depth = 0;

static Display	*print;

static void
print_null(size_t size, char *filename) { }

static void
print_normal(size_t size, char *filename)
{
    fprintf(stdout, "%-7d %s\n", (size >> 1), filename);
}

static void
print_summary(size_t size, char *filename)
{
    if (du_depth == 1) { 
	print_normal(size, filename); 
    }
}


/* tiny recursive du */
static size_t
du(char *filename)
{
    struct stat statbuf;
    size_t	sum;

    if ((lstat(filename, &statbuf)) != 0) { 
	fprintf(stdout, "du: %s: %s\n", filename, strerror(errno));
	return 0; 
    }

    du_depth++;
    sum = statbuf.st_blocks;

    if (S_ISDIR(statbuf.st_mode)) {
	DIR		*dir;
	struct dirent	*entry;

	dir = opendir(filename);
	if (!dir) { return 0; }
	while ((entry = readdir(dir))) {
	    char newfile[512];
	    char *name = entry->d_name;

	    if (  (strcmp(name, "..") == 0)
	       || (strcmp(name, ".")  == 0)) 
	    { continue; }

	    sprintf(newfile, "%s/%s", filename, name);
	    sum += du(newfile);
	}
	closedir(dir);
	print(sum, filename);
    }
    du_depth--;
    return sum;
}

int 
du_main(int argc, char **argv)
{
    int i;
    char opt;

    /* default behaviour */
    print = print_normal;

    /* parse argv[] */
    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    opt = argv[i][1];
	    switch (opt) {
		case 's':
		    print = print_summary;
		    break;
		case 'h':
		    usage(du_usage);
		    break;
		default:
		    fprintf(stderr, "du: invalid option -- %c\n", opt);
		    usage(du_usage);
	    }
	} else {
	    break;
	}
    }

    /* go through remaining args (if any) */
    if (i >= argc) {
	du(".");
    } else {
	int sum;
	for ( ; i < argc; i++) {
	    sum = du(argv[i]);
	    if (sum) { print_normal(sum, argv[i]); }
	}
    }

    exit(0);
}

/* $Id: du.c,v 1.6 1999/12/15 18:52:17 beppu Exp $ */
