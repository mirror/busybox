/* vi: set sw=4 ts=4: */
/*
 * Mini sort implementation for busybox
 *
 *
 * Copyright (C) 1999,2000 by Lineo, inc.
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

static const char sort_usage[] = "sort [-n]"
#ifdef BB_FEATURE_SORT_REVERSE
" [-r]"
#endif
" [FILE]...\n\n";

#ifdef BB_FEATURE_SORT_REVERSE
#define APPLY_REVERSE(x) (reverse ? -(x) : (x))
static int reverse = 0;
#else
#define APPLY_REVERSE(x) (x)
#endif

/* typedefs _______________________________________________________________ */

/* line node */
typedef struct Line {
	char *data;					/* line data */
	struct Line *next;			/* pointer to next line node */
} Line;

/* singly-linked list of lines */
typedef struct {
	int len;					/* number of Lines */
	Line **sorted;				/* array fed to qsort */

	Line *head;					/* head of List */
	Line *current;				/* current Line */
} List;

/* comparison function */
typedef int (Compare) (const void *, const void *);


/* methods ________________________________________________________________ */

static const int max = 1024;

/* mallocate Line */
static Line *line_alloc()
{
	Line *self;

	self = malloc(1 * sizeof(Line));
	return self;
}

/* Initialize Line with string */
static Line *line_init(Line * self, const char *string)
{
	self->data = malloc((strlen(string) + 1) * sizeof(char));

	if (self->data == NULL) {
		return NULL;
	}
	strcpy(self->data, string);
	self->next = NULL;
	return self;
}

/* Construct Line from FILE* */
static Line *line_newFromFile(FILE * src)
{
	char buffer[max];
	Line *self;

	if (fgets(buffer, max, src)) {
		self = line_alloc();
		if (self == NULL) {
			return NULL;
		}
		line_init(self, buffer);
		return self;
	}
	return NULL;
}

/* Line destructor */
static Line *line_release(Line * self)
{
	if (self->data) {
		free(self->data);
		free(self);
	}
	return self;
}


/* Comparison */

/* ascii order */
static int compare_ascii(const void *a, const void *b)
{
	Line **doh;
	Line *x, *y;

	doh = (Line **) a;
	x = *doh;
	doh = (Line **) b;
	y = *doh;

	// fprintf(stdout, "> %p: %s< %p: %s", x, x->data, y, y->data);
	return APPLY_REVERSE(strcmp(x->data, y->data));
}

/* numeric order */
static int compare_numeric(const void *a, const void *b)
{
	Line **doh;
	Line *x, *y;
	int xint, yint;

	doh = (Line **) a;
	x = *doh;
	doh = (Line **) b;
	y = *doh;

	xint = strtoul(x->data, NULL, 10);
	yint = strtoul(y->data, NULL, 10);

	return APPLY_REVERSE(xint - yint);
}


/* List */

/* */
static List *list_init(List * self)
{
	self->len = 0;
	self->sorted = NULL;
	self->head = NULL;
	self->current = NULL;
	return self;
}

/* for simplicity, the List gains ownership of the line */
static List *list_insert(List * self, Line * line)
{
	if (line == NULL) {
		return NULL;
	}

	/* first insertion */
	if (self->head == NULL) {
		self->head = line;
		self->current = line;

		/* all subsequent insertions */
	} else {
		self->current->next = line;
		self->current = line;
	}
	self->len++;
	return self;
}

/* order the list according to compare() */
static List *list_sort(List * self, Compare * compare)
{
	int i;
	Line *line;

	/* mallocate array of Line*s */
	self->sorted = (Line **) malloc(self->len * sizeof(Line *));
	if (self->sorted == NULL) {
		return NULL;
	}

	/* fill array w/ List's contents */
	i = 0;
	line = self->head;
	while (line) {
		self->sorted[i++] = line;
		line = line->next;
	}

	/* apply qsort */
	qsort(self->sorted, self->len, sizeof(Line *), compare);
	return self;
}

/* precondition:  list must be sorted */
static List *list_writeToFile(List * self, FILE * dst)
{
	int i;
	Line **line = self->sorted;

	if (self->sorted == NULL) {
		return NULL;
	}
	for (i = 0; i < self->len; i++) {
		fprintf(dst, "%s", line[i]->data);
	}
	return self;
}

/* deallocate */
static void list_release(List * self)
{
	Line *i;
	Line *die;

	i = self->head;
	while (i) {
		die = i;
		i = die->next;
		line_release(die);
	}
}


/*
 * I need a list
 * to insert lines into
 * then I need to sort this list
 * and finally print it
 */

int sort_main(int argc, char **argv)
{
	int i;
	char opt;
	List list;
	Line *l;
	Compare *compare;

	/* init */
	compare = compare_ascii;
	list_init(&list);

	/* parse argv[] */
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			opt = argv[i][1];
			switch (opt) {
			case 'h':
				usage(sort_usage);
				break;
			case 'n':
				/* numeric comparison */
				compare = compare_numeric;
				break;
#ifdef BB_FEATURE_SORT_REVERSE
			case 'r':
				/* reverse */
				reverse = 1;
				break;
#endif
			default:
				fprintf(stderr, "sort: invalid option -- %c\n", opt);
				usage(sort_usage);
			}
		} else {
			break;
		}
	}

	/* this could be factored better */

	/* work w/ stdin */
	if (i >= argc) {
		while ((l = line_newFromFile(stdin))) {
			list_insert(&list, l);
		}
		list_sort(&list, compare);
		list_writeToFile(&list, stdout);
		list_release(&list);

		/* work w/ what's left in argv[] */
	} else {
		FILE *src;

		for (; i < argc; i++) {
			src = fopen(argv[i], "r");
			if (src == NULL) {
				break;
			}
			while ((l = line_newFromFile(src))) {
				list_insert(&list, l);
			}
			fclose(src);
		}
		list_sort(&list, compare);
		list_writeToFile(&list, stdout);
		list_release(&list);
	}

	exit(0);
}

/* $Id: sort.c,v 1.13 2000/04/13 01:18:56 erik Exp $ */
