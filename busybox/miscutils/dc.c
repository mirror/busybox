/* vi: set sw=4 ts=4: */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "busybox.h"

/* Tiny RPN calculator, because "expr" didn't give me bitwise operations. */

static double stack[100];
static unsigned int pointer;

static void push(double a)
{
	if (pointer >= (sizeof(stack) / sizeof(*stack)))
		error_msg_and_die("stack overflow");
	stack[pointer++] = a;
}

static double pop()
{
	if (pointer == 0)
		error_msg_and_die("stack underflow");
	return stack[--pointer];
}

static void add()
{
	push(pop() + pop());
}

static void sub()
{
	double subtrahend = pop();

	push(pop() - subtrahend);
}

static void mul()
{
	push(pop() * pop());
}

static void divide()
{
	double divisor = pop();

	push(pop() / divisor);
}

static void and()
{
	push((unsigned int) pop() & (unsigned int) pop());
}

static void or()
{
	push((unsigned int) pop() | (unsigned int) pop());
}

static void eor()
{
	push((unsigned int) pop() ^ (unsigned int) pop());
}

static void not()
{
	push(~(unsigned int) pop());
}

static void print()
{
	printf("%g\n", pop());
}

struct op {
	const char *name;
	void (*function) ();
};

static const struct op operators[] = {
	{"+",   add},
	{"add", add},
	{"-",   sub},
	{"sub", sub},
	{"*",   mul},
	{"mul", mul},
	{"/",   divide},
	{"div", divide},
	{"and", and},
	{"or",  or},
	{"not", not},
	{"eor", eor},
	{0,     0}
};

static void stack_machine(const char *argument)
{
	char *endPointer = 0;
	double d;
	const struct op *o = operators;

	if (argument == 0) {
		print();
		return;
	}

	d = strtod(argument, &endPointer);

	if (endPointer != argument) {
		push(d);
		return;
	}

	while (o->name != 0) {
		if (strcmp(o->name, argument) == 0) {
			(*(o->function)) ();
			return;
		}
		o++;
	}
	error_msg_and_die("%s: syntax error.", argument);
}

/* return pointer to next token in buffer and set *buffer to one char
 * past the end of the above mentioned token 
 */
static char *get_token(char **buffer)
{
	char *start   = NULL;
	char *current = *buffer;

	while (isspace(*current)) { current++; }
	if (*current != 0) {
		start = current;
		while (!isspace(*current) && current != 0) { current++; }
		*buffer = current;
	}
	return start;
}

/* In Perl one might say, scalar m|\s*(\S+)\s*|g */
static int number_of_tokens(char *buffer)
{
	int   i = 0;
	char *b = buffer;
	while (get_token(&b)) { i++; }
	return i;
}

int dc_main(int argc, char **argv)
{
	/* take stuff from stdin if no args are given */
	if (argc <= 1) {
		int i, len;
		char *line   = NULL;
		char *cursor = NULL;
		char *token  = NULL;
		while ((line = get_line_from_file(stdin))) {
			cursor = line;
			len = number_of_tokens(line);
			for (i = 0; i < len; i++) {
				token = get_token(&cursor);
				*cursor++ = 0;
				stack_machine(token);
			}
			free(line);
		}
	} else {
		if (*argv[1]=='-')
			show_usage();
		while (argc >= 2) {
			stack_machine(argv[1]);
			argv++;
			argc--;
		}
	}
	stack_machine(0);
	return EXIT_SUCCESS;
}
