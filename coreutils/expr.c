/* vi: set sw=4 ts=4: */
/*
 * Mini expr implementation for busybox
 *
 * based on GNU expr Mike Parker.
 * Copyright (C) 86, 1991-1997, 1999 Free Software Foundation, Inc.
 *
 * Busybox modifications 
 * Copyright (c) 2000  Edward Betts <edward@debian.org>.
 *
 * this program is free software; you can redistribute it and/or modify
 * it under the terms of the gnu general public license as published by
 * the free software foundation; either version 2 of the license, or
 * (at your option) any later version.
 *
 * this program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * merchantability or fitness for a particular purpose. see the gnu
 * general public license for more details.
 *
 * you should have received a copy of the gnu general public license
 * along with this program; if not, write to the free software
 * foundation, inc., 59 temple place, suite 330, boston, ma 02111-1307 usa
 *
 */

/* This program evaluates expressions.  Each token (operator, operand,
 * parenthesis) of the expression must be a seperate argument.  The
 * parser used is a reasonably general one, though any incarnation of
 * it is language-specific.  It is especially nice for expressions.
 *
 * No parse tree is needed; a new node is evaluated immediately.
 * One function can handle multiple operators all of equal precedence,
 * provided they all associate ((x op x) op x). */

/* no getopt needed */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <regex.h>
#include <sys/types.h>
#include "busybox.h"


/* The kinds of value we can have.  */
enum valtype {
	integer,
	string
};
typedef enum valtype TYPE;

/* A value is.... */
struct valinfo {
	TYPE type;			/* Which kind. */
	union {				/* The value itself. */
		int i;
		char *s;
	} u;
};
typedef struct valinfo VALUE;

/* The arguments given to the program, minus the program name.  */
static char **args;

static VALUE *docolon (VALUE *sv, VALUE *pv);
static VALUE *eval (void);
static VALUE *int_value (int i);
static VALUE *str_value (char *s);
static int nextarg (char *str);
static int null (VALUE *v);
static int toarith (VALUE *v);
static void freev (VALUE *v);
static void tostring (VALUE *v);

int expr_main (int argc, char **argv)
{
	VALUE *v;

	if (argc == 1) {
		error_msg_and_die("too few arguments");
	}

	args = argv + 1;

	v = eval ();
	if (*args)
		error_msg_and_die ("syntax error");

	if (v->type == integer)
		printf ("%d\n", v->u.i);
	else
		puts (v->u.s);

	exit (null (v));
}

/* Return a VALUE for I.  */

static VALUE *int_value (int i)
{
	VALUE *v;

	v = xmalloc (sizeof(VALUE));
	v->type = integer;
	v->u.i = i;
	return v;
}

/* Return a VALUE for S.  */

static VALUE *str_value (char *s)
{
	VALUE *v;

	v = xmalloc (sizeof(VALUE));
	v->type = string;
	v->u.s = strdup (s);
	return v;
}

/* Free VALUE V, including structure components.  */

static void freev (VALUE *v)
{
	if (v->type == string)
		free (v->u.s);
	free (v);
}

/* Return nonzero if V is a null-string or zero-number.  */

static int null (VALUE *v)
{
	switch (v->type) {
		case integer:
			return v->u.i == 0;
		case string:
			return v->u.s[0] == '\0' || strcmp (v->u.s, "0") == 0;
		default:
			abort ();
	}
}

/* Coerce V to a string value (can't fail).  */

static void tostring (VALUE *v)
{
	char *temp;

	if (v->type == integer) {
		temp = xmalloc (4 * (sizeof (int) / sizeof (char)));
		sprintf (temp, "%d", v->u.i);
		v->u.s = temp;
		v->type = string;
	}
}

/* Coerce V to an integer value.  Return 1 on success, 0 on failure.  */

static int toarith (VALUE *v)
{
	int i;

	switch (v->type) {
		case integer:
			return 1;
		case string:
			i = 0;
			/* Don't interpret the empty string as an integer.  */
			if (v->u.s == 0)
				return 0;
			i = atoi(v->u.s);
			free (v->u.s);
			v->u.i = i;
			v->type = integer;
			return 1;
		default:
			abort ();
	}
}

/* Return nonzero if the next token matches STR exactly.
   STR must not be NULL.  */

static int
nextarg (char *str)
{
	if (*args == NULL)
		return 0;
	return strcmp (*args, str) == 0;
}

/* The comparison operator handling functions.  */

#define cmpf(name, rel)					\
static int name (VALUE *l, VALUE *r)		\
{							\
	if (l->type == string || r->type == string) {		\
		tostring (l);				\
		tostring (r);				\
		return strcmp (l->u.s, r->u.s) rel 0;	\
	}						\
	else						\
		return l->u.i rel r->u.i;		\
}
 cmpf (less_than, <)
 cmpf (less_equal, <=)
 cmpf (equal, ==)
 cmpf (not_equal, !=)
 cmpf (greater_equal, >=)
 cmpf (greater_than, >)

#undef cmpf

/* The arithmetic operator handling functions.  */

#define arithf(name, op)			\
static						\
int name (VALUE *l, VALUE *r)		\
{						\
  if (!toarith (l) || !toarith (r))		\
    error_msg_and_die ("non-numeric argument");	\
  return l->u.i op r->u.i;			\
}

#define arithdivf(name, op)			\
static int name (VALUE *l, VALUE *r)		\
{						\
  if (!toarith (l) || !toarith (r))		\
    error_msg_and_die ( "non-numeric argument");	\
  if (r->u.i == 0)				\
    error_msg_and_die ( "division by zero");		\
  return l->u.i op r->u.i;			\
}

 arithf (plus, +)
 arithf (minus, -)
 arithf (multiply, *)
 arithdivf (divide, /)
 arithdivf (mod, %)

#undef arithf
#undef arithdivf

/* Do the : operator.
   SV is the VALUE for the lhs (the string),
   PV is the VALUE for the rhs (the pattern).  */

static VALUE *docolon (VALUE *sv, VALUE *pv)
{
	VALUE *v;
	const char *errmsg;
	struct re_pattern_buffer re_buffer;
	struct re_registers re_regs;
	int len;

	tostring (sv);
	tostring (pv);

	if (pv->u.s[0] == '^') {
		fprintf (stderr, "\
warning: unportable BRE: `%s': using `^' as the first character\n\
of a basic regular expression is not portable; it is being ignored",
		pv->u.s);
	}

	len = strlen (pv->u.s);
	memset (&re_buffer, 0, sizeof (re_buffer));
	memset (&re_regs, 0, sizeof (re_regs));
	re_buffer.allocated = 2 * len;
	re_buffer.buffer = (unsigned char *) xmalloc (re_buffer.allocated);
	re_buffer.translate = 0;
	re_syntax_options = RE_SYNTAX_POSIX_BASIC;
	errmsg = re_compile_pattern (pv->u.s, len, &re_buffer);
	if (errmsg) {
		error_msg_and_die("%s", errmsg);
	}

	len = re_match (&re_buffer, sv->u.s, strlen (sv->u.s), 0, &re_regs);
	if (len >= 0) {
		/* Were \(...\) used? */
		if (re_buffer.re_nsub > 0) { /* was (re_regs.start[1] >= 0) */
			sv->u.s[re_regs.end[1]] = '\0';
			v = str_value (sv->u.s + re_regs.start[1]);
		}
		else
			v = int_value (len);
	}
	else {
		/* Match failed -- return the right kind of null.  */
		if (re_buffer.re_nsub > 0)
			v = str_value ("");
		else
			v = int_value (0);
	}
	free (re_buffer.buffer);
	return v;
}

/* Handle bare operands and ( expr ) syntax.  */

static VALUE *eval7 (void)
{
	VALUE *v;

	if (!*args)
		error_msg_and_die ( "syntax error");

	if (nextarg ("(")) {
		args++;
		v = eval ();
		if (!nextarg (")"))
			error_msg_and_die ( "syntax error");
			args++;
			return v;
		}

	if (nextarg (")"))
		error_msg_and_die ( "syntax error");

	return str_value (*args++);
}

/* Handle match, substr, index, length, and quote keywords.  */

static VALUE *eval6 (void)
{
	VALUE *l, *r, *v, *i1, *i2;

	if (nextarg ("quote")) {
		args++;
		if (!*args)
			error_msg_and_die ( "syntax error");
		return str_value (*args++);
	}
	else if (nextarg ("length")) {
		args++;
		r = eval6 ();
		tostring (r);
		v = int_value (strlen (r->u.s));
		freev (r);
		return v;
	}
	else if (nextarg ("match")) {
		args++;
		l = eval6 ();
		r = eval6 ();
		v = docolon (l, r);
		freev (l);
		freev (r);
		return v;
	}
	else if (nextarg ("index")) {
		args++;
		l = eval6 ();
		r = eval6 ();
		tostring (l);
		tostring (r);
		v = int_value (strcspn (l->u.s, r->u.s) + 1);
		if (v->u.i == (int) strlen (l->u.s) + 1)
			v->u.i = 0;
		freev (l);
		freev (r);
		return v;
	}
	else if (nextarg ("substr")) {
		args++;
		l = eval6 ();
		i1 = eval6 ();
		i2 = eval6 ();
		tostring (l);
		if (!toarith (i1) || !toarith (i2)
			|| i1->u.i > (int) strlen (l->u.s)
			|| i1->u.i <= 0 || i2->u.i <= 0)
		v = str_value ("");
		else {
			v = xmalloc (sizeof(VALUE));
			v->type = string;
			v->u.s = strncpy ((char *) xmalloc (i2->u.i + 1),
				l->u.s + i1->u.i - 1, i2->u.i);
				v->u.s[i2->u.i] = 0;
		}
		freev (l);
		freev (i1);
		freev (i2);
		return v;
	}
	else
		return eval7 ();
}

/* Handle : operator (pattern matching).
   Calls docolon to do the real work.  */

static VALUE *eval5 (void)
{
	VALUE *l, *r, *v;

	l = eval6 ();
	while (nextarg (":")) {
		args++;
		r = eval6 ();
		v = docolon (l, r);
		freev (l);
		freev (r);
		l = v;
	}
	return l;
}

/* Handle *, /, % operators.  */

static VALUE *eval4 (void)
{
	VALUE *l, *r;
	int (*fxn) (VALUE *, VALUE *), val;

	l = eval5 ();
	while (1) {
		if (nextarg ("*"))
			fxn = multiply;
		else if (nextarg ("/"))
			fxn = divide;
		else if (nextarg ("%"))
			fxn = mod;
		else
			return l;
		args++;
		r = eval5 ();
		val = (*fxn) (l, r);
		freev (l);
		freev (r);
		l = int_value (val);
	}
}

/* Handle +, - operators.  */

static VALUE *eval3 (void)
{
	VALUE *l, *r;
	int (*fxn) (VALUE *, VALUE *), val;

	l = eval4 ();
	while (1) {
		if (nextarg ("+"))
			fxn = plus;
		else if (nextarg ("-"))
			fxn = minus;
		else
			return l;
		args++;
		r = eval4 ();
		val = (*fxn) (l, r);
		freev (l);
		freev (r);
		l = int_value (val);
	}
}

/* Handle comparisons.  */

static VALUE *eval2 (void)
{
	VALUE *l, *r;
	int (*fxn) (VALUE *, VALUE *), val;

	l = eval3 ();
	while (1) {
		if (nextarg ("<"))
			fxn = less_than;
		else if (nextarg ("<="))
			fxn = less_equal;
		else if (nextarg ("=") || nextarg ("=="))
			fxn = equal;
		else if (nextarg ("!="))
			fxn = not_equal;
		else if (nextarg (">="))
			fxn = greater_equal;
		else if (nextarg (">"))
			fxn = greater_than;
		else
			return l;
		args++;
		r = eval3 ();
		toarith (l);
		toarith (r);
		val = (*fxn) (l, r);
		freev (l);
		freev (r);
		l = int_value (val);
	}
}

/* Handle &.  */

static VALUE *eval1 (void)
{
	VALUE *l, *r;

	l = eval2 ();
	while (nextarg ("&")) {
		args++;
		r = eval2 ();
		if (null (l) || null (r)) {
			freev (l);
			freev (r);
			l = int_value (0);
		}
		else
			freev (r);
	}
	return l;
}

/* Handle |.  */

static VALUE *eval (void)
{
	VALUE *l, *r;

	l = eval1 ();
	while (nextarg ("|")) {
		args++;
		r = eval1 ();
		if (null (l)) {
			freev (l);
			l = r;
		}
		else
			freev (r);
	}
	return l;
}
