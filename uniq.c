/*
 * Mini uniq implementation for busybox
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
#include <stdio.h>

static const char uniq_usage[] =
"haha\n"
;

/* max chars in line */
#define UNIQ_MAX 4096

typedef void (Print)(FILE *, const char *);

typedef int (Decide)(const char *, const char *);

/* container for two lines to be compared */
typedef struct {
    char    *a;
    char    *b;
    int	    recurrence;
    FILE    *in;
    FILE    *out;
    void    *func;
} Subject;

/* set up all the variables of a uniq operation */
static Subject *
subject_init(Subject *self, FILE *in, FILE *out, void *func)
{
    self->a    = NULL;
    self->b    = NULL;
    self->in   = in;
    self->out  = out;
    self->func = func;
    self->recurrence = 0;
    return self;
}

/* point a and b to the appropriate lines;
 * count the recurrences (if any) of a string;
 */
static Subject *
subject_next(Subject *self)
{
    /* tmp line holders */
    static char line[2][UNIQ_MAX];
    static int  alternator = 0;

    if (fgets(line[alternator], UNIQ_MAX, self->in)) {
	self->a = self->b;
	self->b = line[alternator];
	alternator ^= 1;
	return self;
    }

    return NULL;
}

static Subject *
subject_last(Subject *self)
{
    self->a = self->b;
    self->b = NULL;
    return self;
}

static Subject *
subject_study(Subject *self)
{
    if (self->a == NULL) {
	return self;
    }
    if (self->b == NULL) {
	fprintf(self->out, "%s", self->a);
	return self;
    }
    if (strcmp(self->a, self->b) == 0) {
	self->recurrence++;
    } else {
	fprintf(self->out, "%s", self->a);
	self->recurrence = 0;
    }
    return self;
}

/* one variable is the decision algo */
/* another variable is the printing algo */

/* I don't think I have to have more than a 1 line memory 
   this is the one constant */

/* it seems like GNU/uniq only takes one or two files as an option */

/* ________________________________________________________________________ */
int
uniq_main(int argc, char **argv)
{
    int	    i;
    char    opt;
    FILE    *in, *out;
    Subject s;

    /* init */
    in  = stdin;
    out = stdout;

    subject_init(&s, in, out, NULL);
    while (subject_next(&s)) { 
	subject_study(&s);
    }
    subject_last(&s);
    subject_study(&s);
    exit(0);

    /* XXX : finish the tedious stuff */

    /* parse argv[] */
    for (i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    opt = argv[i][1];
	    switch (opt) {
		case '-':
		case 'h':
		    usage(uniq_usage);
		default:
		    usage(uniq_usage);
	    }
	} else {
	    break;
	}
    }

    exit(0);
}

/* $Id: uniq.c,v 1.2 2000/01/06 01:14:56 erik Exp $ */
