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

/* structs ________________________________________________________________ */

/* line node */
typedef struct {
    char	*data;	    /* line data */
    struct Line *next;	    /* pointer to next line node */
} Line;

/* singly-linked list of lines */
typedef struct {
    int		len;	    /* number of Lines */
    Line	*sorted;    /* array fed to qsort */

    Line	*head;	    /* head of List */
    Line	*current    /* current Line */
} List;


/* methods ________________________________________________________________ */

static const int max = 1024;

/* mallocate Line */
static Line *
line_alloc()
{
    Line *self;
    self = malloc(1 * sizeof(Line));
    return self;
}

/* Initialize Line with string */
static Line *
line_init(Line *self, const char *string)
{
    self->data = malloc((strlen(string) + 1) * sizeof(char));
    if (self->data == NULL) { return NULL; }
    strcpy(self->data, string);
    self->next = NULL;
    return self;
}

/* Construct Line from FILE* */
static Line *
line_newFromFile(FILE *src)
{
    char    buffer[max];
    Line    *self;

    if (fgets(buffer, max, src)) {
	self = line_alloc();
	if (self == NULL) { return NULL; }
	line_init(self, buffer);
	return self;
    }
    return NULL;
}

/* Line destructor */
static Line *
line_release(Line *self)
{
    if (self->data) { 
	free(self->data); 
	free(self);
    }
    return self;
}


/* Comparison */

static int
compare_ascii(const void *, const void *);

static int
compare_numeric(const void *, const void *);


/* List */

/* */
static List *
list_init(List *self)
{
    self->len     = 0;
    self->sorted  = NULL;
    self->head    = NULL;
    self->current = NULL;
    return self;
}

/* for simplicity, the List gains ownership of the line */
static void
list_insert(List *self, Line *line)
{
    if (line == NULL) { return NULL; }

    /* first insertion */
    if (self->head == NULL) {
	self->head    = line;
	self->current = line;

    /* all subsequent insertions */
    } else {
	self->current->next = line;
	self->current       = line;
    }
    self->len++;
    return self;
}

/* */
static List *
list_sort(List *self);

/* precondition:  list must be sorted */
static List *
list_writeToFile(List *self, FILE* dst)
{
    if (self->sorted == NULL) { return NULL; }
}

/* deallocate */
static List *
list_release(List *self)
{
    Line *i;
    Line *die;

    i = self->head;
    while (i) {
	die = i;
	i = die->next;
	line_delete(die);
    }
    return self; /* bad poetry? */
}


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

/* $Id: sort.c,v 1.3 1999/12/22 17:57:31 beppu Exp $ */
