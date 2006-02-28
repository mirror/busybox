/* vi: set sw=4 ts=4: */
/* printf - format and print data

   Copyright 1999 Dave Cinege
   Portions copyright (C) 1990-1996 Free Software Foundation, Inc.

   Licensed under GPL v2 or later, see file LICENSE in this tarball for details.
*/

/* Usage: printf format [argument...]

   A front end to the printf function that lets it be used from the shell.

   Backslash escapes:

   \" = double quote
   \\ = backslash
   \a = alert (bell)
   \b = backspace
   \c = produce no further output
   \f = form feed
   \n = new line
   \r = carriage return
   \t = horizontal tab
   \v = vertical tab
   \0ooo = octal number (ooo is 0 to 3 digits)
   \xhhh = hexadecimal number (hhh is 1 to 3 digits)

   Additional directive:

   %b = print an argument string, interpreting backslash escapes

   The `format' argument is re-used as many times as necessary
   to convert all of the given arguments.

   David MacKenzie <djm@gnu.ai.mit.edu> */


//   19990508 Busy Boxed! Dave Cinege

#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#include <ctype.h>
#include <assert.h>
#include "busybox.h"

static int print_formatted (char *format, int argc, char **argv);
static void print_direc (char *start, size_t length,
			int field_width, int precision, char *argument);

typedef int (*converter)(char *arg, void *result);
void multiconvert(char *arg, void *result, converter convert)
{
	char s[16];
	if (*arg == '"' || *arg == '\'') {
		sprintf(s,"%d",(unsigned)*(++arg));
		arg=s;
	}
	if(convert(arg,result)) fprintf(stderr, "%s", arg);
}

static unsigned long xstrtoul(char *arg)
{
	unsigned long result;

	multiconvert(arg,&result, (converter)safe_strtoul);
	return result;
}

static long xstrtol(char *arg)
{
	long result;
	multiconvert(arg, &result, (converter)safe_strtol);
	return result;
}

static double xstrtod(char *arg)
{
	double result;
	multiconvert(arg, &result, (converter)safe_strtod);
	return result;
}

static void print_esc_string(char *str)
{
	for (; *str; str++) {
		if (*str == '\\') {
			str++;
			putchar(bb_process_escape_sequence((const char **)&str));
		} else {
			putchar(*str);
		}

	}
}

int printf_main(int argc, char **argv)
{
	char *format;
	int args_used;

	if (argc <= 1 || **(argv + 1) == '-') {
		bb_show_usage();
	}

	format = argv[1];
	argc -= 2;
	argv += 2;

	do {
		args_used = print_formatted(format, argc, argv);
		argc -= args_used;
		argv += args_used;
	}
	while (args_used > 0 && argc > 0);

/*
  if (argc > 0)
    fprintf(stderr, "excess args ignored");
*/

	return EXIT_SUCCESS;
}

/* Print the text in FORMAT, using ARGV (with ARGC elements) for
   arguments to any `%' directives.
   Return the number of elements of ARGV used.  */

static int print_formatted(char *format, int argc, char **argv)
{
	int save_argc = argc;		/* Preserve original value.  */
	char *f;					/* Pointer into `format'.  */
	char *direc_start;			/* Start of % directive.  */
	size_t direc_length;		/* Length of % directive.  */
	int field_width;			/* Arg to first '*', or -1 if none.  */
	int precision;				/* Arg to second '*', or -1 if none.  */

	for (f = format; *f; ++f) {
		switch (*f) {
		case '%':
			direc_start = f++;
			direc_length = 1;
			field_width = precision = -1;
			if (*f == '%') {
				putchar('%');
				break;
			}
			if (*f == 'b') {
				if (argc > 0) {
					print_esc_string(*argv);
					++argv;
					--argc;
				}
				break;
			}
			if (strchr("-+ #", *f)) {
				++f;
				++direc_length;
			}
			if (*f == '*') {
				++f;
				++direc_length;
				if (argc > 0) {
					field_width = xstrtoul(*argv);
					++argv;
					--argc;
				} else
					field_width = 0;
			} else
				while (isdigit(*f)) {
					++f;
					++direc_length;
				}
			if (*f == '.') {
				++f;
				++direc_length;
				if (*f == '*') {
					++f;
					++direc_length;
					if (argc > 0) {
						precision = xstrtoul(*argv);
						++argv;
						--argc;
					} else
						precision = 0;
				} else
					while (isdigit(*f)) {
						++f;
						++direc_length;
					}
			}
			if (*f == 'l' || *f == 'L' || *f == 'h') {
				++f;
				++direc_length;
			}
			/*
			   if (!strchr ("diouxXfeEgGcs", *f))
			   fprintf(stderr, "%%%c: invalid directive", *f);
			 */
			++direc_length;
			if (argc > 0) {
				print_direc(direc_start, direc_length, field_width,
							precision, *argv);
				++argv;
				--argc;
			} else
				print_direc(direc_start, direc_length, field_width,
							precision, "");
			break;

		case '\\':
			if (*++f == 'c')
				exit(0);
			putchar(bb_process_escape_sequence((const char **)&f));
			f--;
			break;

		default:
			putchar(*f);
		}
	}

	return save_argc - argc;
}

static void
print_direc(char *start, size_t length, int field_width, int precision,
			char *argument)
{
	char *p;					/* Null-terminated copy of % directive. */

	p = xmalloc((unsigned) (length + 1));
	strncpy(p, start, length);
	p[length] = 0;

	switch (p[length - 1]) {
	case 'd':
	case 'i':
		if (field_width < 0) {
			if (precision < 0)
				printf(p, xstrtol(argument));
			else
				printf(p, precision, xstrtol(argument));
		} else {
			if (precision < 0)
				printf(p, field_width, xstrtol(argument));
			else
				printf(p, field_width, precision, xstrtol(argument));
		}
		break;

	case 'o':
	case 'u':
	case 'x':
	case 'X':
		if (field_width < 0) {
			if (precision < 0)
				printf(p, xstrtoul(argument));
			else
				printf(p, precision, xstrtoul(argument));
		} else {
			if (precision < 0)
				printf(p, field_width, xstrtoul(argument));
			else
				printf(p, field_width, precision, xstrtoul(argument));
		}
		break;

	case 'f':
	case 'e':
	case 'E':
	case 'g':
	case 'G':
		if (field_width < 0) {
			if (precision < 0)
				printf(p, xstrtod(argument));
			else
				printf(p, precision, xstrtod(argument));
		} else {
			if (precision < 0)
				printf(p, field_width, xstrtod(argument));
			else
				printf(p, field_width, precision, xstrtod(argument));
		}
		break;

	case 'c':
		printf(p, *argument);
		break;

	case 's':
		if (field_width < 0) {
			if (precision < 0)
				printf(p, argument);
			else
				printf(p, precision, argument);
		} else {
			if (precision < 0)
				printf(p, field_width, argument);
			else
				printf(p, field_width, precision, argument);
		}
		break;
	}

	free(p);
}
