/* vi: set sw=4 ts=4: */
/*
 * Mini expr implementation for busybox
 *
 * based on GNU expr Mike Parker.
 * Copyright (C) 86, 1991-1997, 1999 Free Software Foundation, Inc.
 *
 * Busybox modifications
 * Copyright (c) 2000  Edward Betts <edward@debian.org>.
 * Aug 2003  Vladimir Oleynik - reduced 464 bytes.
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

/* This program evaluates expressions.  Each token (operator, operand,
 * parenthesis) of the expression must be a separate argument.  The
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
#include <errno.h>
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
		bb_error_msg_and_die("too few arguments");
	}

	args = argv + 1;

	v = eval ();
	if (*args)
		bb_error_msg_and_die ("syntax error");

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
	v->u.s = bb_xstrdup (s);
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
		default: /* string: */
			return v->u.s[0] == '\0' || strcmp (v->u.s, "0") == 0;
	}
}

/* Coerce V to a string value (can't fail).  */

static void tostring (VALUE *v)
{
	if (v->type == integer) {
               bb_xasprintf (&(v->u.s), "%d", v->u.i);
		v->type = string;
	}
}

/* Coerce V to an integer value.  Return 1 on success, 0 on failure.  */

static int toarith (VALUE *v)
{
	if(v->type == string) {
		int i;
		char *e;

		/* Don't interpret the empty string as an integer.  */
		/* Currently does not worry about overflow or int/long differences. */
		i = (int) strtol(v->u.s, &e, 10);
		if ((v->u.s == e) || *e)
			return 0;
		free (v->u.s);
		v->u.i = i;
		v->type = integer;
	}
	return 1;
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

static int cmp_common (VALUE *l, VALUE *r, int op)
{
	int cmpval;

	if (l->type == string || r->type == string) {
		tostring (l);
		tostring (r);
		cmpval = strcmp (l->u.s, r->u.s);
	}
	else
		cmpval = l->u.i - r->u.i;
	switch(op) {
		case '<':
			return cmpval < 0;
		case ('L'+'E'):
			return cmpval <= 0;
		case '=':
			return cmpval == 0;
		case '!':
			return cmpval != 0;
		case '>':
			return cmpval > 0;
		default: /* >= */
			return cmpval >= 0;
	}
}

/* The arithmetic operator handling functions.  */

static int arithmetic_common (VALUE *l, VALUE *r, int op)
{
  int li, ri;

  if (!toarith (l) || !toarith (r))
    bb_error_msg_and_die ("non-numeric argument");
  li = l->u.i;
  ri = r->u.i;
  if((op == '/' || op == '%') && ri == 0)
    bb_error_msg_and_die ( "division by zero");
  switch(op) {
	case '+':
		return li + ri;
	case '-':
		return li - ri;
	case '*':
		return li * ri;
	case '/':
		return li / ri;
	default:
		return li % ri;
  }
}

/* Do the : operator.
   SV is the VALUE for the lhs (the string),
   PV is the VALUE for the rhs (the pattern).  */

static VALUE *docolon (VALUE *sv, VALUE *pv)
{
	VALUE *v;
	regex_t re_buffer;
	const int NMATCH = 2;
	regmatch_t re_regs[NMATCH];

	tostring (sv);
	tostring (pv);

	if (pv->u.s[0] == '^') {
		fprintf (stderr, "\
warning: unportable BRE: `%s': using `^' as the first character\n\
of a basic regular expression is not portable; it is being ignored",
		pv->u.s);
	}

	memset (&re_buffer, 0, sizeof (re_buffer));
	memset (re_regs, 0, sizeof (*re_regs));
	if( regcomp (&re_buffer, pv->u.s, 0) != 0 )
		bb_error_msg_and_die("Invalid regular expression");

	/* expr uses an anchored pattern match, so check that there was a
	 * match and that the match starts at offset 0. */
	if (regexec (&re_buffer, sv->u.s, NMATCH, re_regs, 0) != REG_NOMATCH &&
			re_regs[0].rm_so == 0) {
		/* Were \(...\) used? */
		if (re_buffer.re_nsub > 0) {
			sv->u.s[re_regs[1].rm_eo] = '\0';
			v = str_value (sv->u.s + re_regs[1].rm_so);
		}
		else
			v = int_value (re_regs[0].rm_eo);
	}
	else {
		/* Match failed -- return the right kind of null.  */
		if (re_buffer.re_nsub > 0)
			v = str_value ("");
		else
			v = int_value (0);
	}
	return v;
}

/* Handle bare operands and ( expr ) syntax.  */

static VALUE *eval7 (void)
{
	VALUE *v;

	if (!*args)
		bb_error_msg_and_die ( "syntax error");

	if (nextarg ("(")) {
		args++;
		v = eval ();
		if (!nextarg (")"))
			bb_error_msg_and_die ( "syntax error");
			args++;
			return v;
		}

	if (nextarg (")"))
		bb_error_msg_and_die ( "syntax error");

	return str_value (*args++);
}

/* Handle match, substr, index, length, and quote keywords.  */

static VALUE *eval6 (void)
{
	VALUE *l, *r, *v, *i1, *i2;

	if (nextarg ("quote")) {
		args++;
		if (!*args)
			bb_error_msg_and_die ( "syntax error");
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
                       v->u.s = bb_xstrndup(l->u.s + i1->u.i - 1, i2->u.i);
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
	int op, val;

	l = eval5 ();
	while (1) {
		if (nextarg ("*"))
			op = '*';
		else if (nextarg ("/"))
			op = '/';
		else if (nextarg ("%"))
			op = '%';
		else
			return l;
		args++;
		r = eval5 ();
		val = arithmetic_common (l, r, op);
		freev (l);
		freev (r);
		l = int_value (val);
	}
}

/* Handle +, - operators.  */

static VALUE *eval3 (void)
{
	VALUE *l, *r;
	int op, val;

	l = eval4 ();
	while (1) {
		if (nextarg ("+"))
			op = '+';
		else if (nextarg ("-"))
			op = '-';
		else
			return l;
		args++;
		r = eval4 ();
		val = arithmetic_common (l, r, op);
		freev (l);
		freev (r);
		l = int_value (val);
	}
}

/* Handle comparisons.  */

static VALUE *eval2 (void)
{
	VALUE *l, *r;
	int op, val;

	l = eval3 ();
	while (1) {
		if (nextarg ("<"))
			op = '<';
		else if (nextarg ("<="))
			op = 'L'+'E';
		else if (nextarg ("=") || nextarg ("=="))
			op = '=';
		else if (nextarg ("!="))
			op = '!';
		else if (nextarg (">="))
			op = 'G'+'E';
		else if (nextarg (">"))
			op = '>';
		else
			return l;
		args++;
		r = eval3 ();
		toarith (l);
		toarith (r);
		val = cmp_common (l, r, op);
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
