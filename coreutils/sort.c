/*
 * Mini find implementation for busybox
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

static const char sort_usage[] =
"Usage: sort [OPTION]... [FILE]...\n\n"
;


/* line node */
typedef struct {
    char	*data;	    /* line data */
    struct Line *next;	    /* pointer to next line node */
} Line;

/* singly-linked list of lines */
typedef struct {
    int		len;	    /* number of Lines */
    Line	*line;	    /* array fed to qsort */
    Line	*head;	    /* head of List */
} List;


/* Line methods */

static const int max = 1024;

static Line *
line_new()
{
    char buffer[max];
}


/* Comparison */

static int
compare_ascii(const void *, const void *);

static int
compare_numeric(const void *, const void *);


/* List */

static void
list_insert();

static void
list_sort();

static void
list_print();



/*
 * I need a list
 * to insert lines into
 * then I need to sort this list
 * and finally print it
 */

int 
sort_main(int argc, char **argv)
{
    int i;
    char opt;

    /* default behaviour */

    /* parse argv[] */
    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    opt = argv[i][1];
	    switch (opt) {
		case 'h':
		    usage(sort_usage);
		    break;
		default:
		    fprintf(stderr, "sort: invalid option -- %c\n", opt);
		    usage(sort_usage);
	    }
	} else {
	    break;
	}
    }

    /* go through remaining args (if any) */
    if (i >= argc) {

    } else {
	for ( ; i < argc; i++) {
	}
    }

    exit(0);
}

/* $Date: 1999/12/21 20:00:35 $ */
/* $Id: sort.c,v 1.1 1999/12/21 20:00:35 beppu Exp $ */
