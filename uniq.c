/* vi: set sw=4 ts=4: */
/*
 * Mini uniq implementation for busybox
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
#include <stdio.h>
#include <string.h>
#include <errno.h>

static const char uniq_usage[] =
	"uniq [OPTION]... [INPUT [OUTPUT]]\n\n"
	"Discard all but one of successive identical lines from INPUT\n"
	"(or standard input), writing to OUTPUT (or standard output).\n";

/* max chars in line */
#define UNIQ_MAX 4096

typedef void (Print) (FILE *, const char *);

typedef int (Decide) (const char *, const char *);

/* container for two lines to be compared */
typedef struct {
	char *a;
	char *b;
	int recurrence;
	FILE *in;
	FILE *out;
	void *func;
} Subject;

/* set up all the variables of a uniq operation */
static Subject *subject_init(Subject * self, FILE * in, FILE * out,
							 void *func)
{
	self->a = NULL;
	self->b = NULL;
	self->in = in;
	self->out = out;
	self->func = func;
	self->recurrence = 0;
	return self;
}

/* point a and b to the appropriate lines;
 * count the recurrences (if any) of a string;
 */
static Subject *subject_next(Subject * self)
{
	/* tmp line holders */
	static char line[2][UNIQ_MAX];
	static int alternator = 0;

	if (fgets(line[alternator], UNIQ_MAX, self->in)) {
		self->a = self->b;
		self->b = line[alternator];
		alternator ^= 1;
		return self;
	}

	return NULL;
}

static Subject *subject_last(Subject * self)
{
	self->a = self->b;
	self->b = NULL;
	return self;
}

static Subject *subject_study(Subject * self)
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

static int
set_file_pointers(int schema, FILE ** in, FILE ** out, char **argv)
{
	switch (schema) {
	case 0:
		*in = stdin;
		*out = stdout;
		break;
	case 1:
		*in = fopen(argv[0], "r");
		*out = stdout;
		break;
	case 2:
		*in = fopen(argv[0], "r");
		*out = fopen(argv[1], "w");
		break;
	}
	if (*in == NULL) {
		fprintf(stderr, "uniq: %s: %s\n", argv[0], strerror(errno));
		return errno;
	}
	if (*out == NULL) {
		fprintf(stderr, "uniq: %s: %s\n", argv[1], strerror(errno));
		return errno;
	}
	return 0;
}


/* one variable is the decision algo */
/* another variable is the printing algo */

/* I don't think I have to have more than a 1 line memory 
   this is the one constant */

/* it seems like GNU/uniq only takes one or two files as an option */

/* ________________________________________________________________________ */
int uniq_main(int argc, char **argv)
{
	int i;
	char opt;
	FILE *in, *out;
	Subject s;

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

	/* 0 src: stdin; dst: stdout */
	/* 1 src: file;  dst: stdout */
	/* 2 src: file;  dst: file   */
	if (set_file_pointers((argc - 1), &in, &out, &argv[i])) {
		exit(1);
	}

	subject_init(&s, in, out, NULL);
	while (subject_next(&s)) {
		subject_study(&s);
	}
	subject_last(&s);
	subject_study(&s);

	exit(0);
}

/* $Id: uniq.c,v 1.9 2000/04/17 16:16:10 erik Exp $ */
