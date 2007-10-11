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

   The 'format' argument is re-used as many times as necessary
   to convert all of the given arguments.

   David MacKenzie <djm@gnu.ai.mit.edu> */


//   19990508 Busy Boxed! Dave Cinege

#include "libbb.h"

typedef void (*converter)(const char *arg, void *result);

static void multiconvert(const char *arg, void *result, converter convert)
{
	char s[sizeof(int)*3 + 2];

	if (*arg == '"' || *arg == '\'') {
		sprintf(s, "%d", (unsigned char)arg[1]);
		arg = s;
	}
	convert(arg, result);
	/* if there was conversion error, print unconverted string */
	if (errno)
		fputs(arg, stderr);
}

static void conv_strtoul(const char *arg, void *result)
{
	*(unsigned long*)result = bb_strtoul(arg, NULL, 0);
}
static void conv_strtol(const char *arg, void *result)
{
	*(long*)result = bb_strtol(arg, NULL, 0);
}
static void conv_strtod(const char *arg, void *result)
{
	char *end;
	/* Well, this one allows leading whitespace... so what */
	/* What I like much less is that "-" is accepted too! :( */
	*(double*)result = strtod(arg, &end);
	if (end[0]) errno = ERANGE;
}

static unsigned long my_xstrtoul(const char *arg)
{
	unsigned long result;
	multiconvert(arg, &result, conv_strtoul);
	return result;
}

static long my_xstrtol(const char *arg)
{
	long result;
	multiconvert(arg, &result, conv_strtol);
	return result;
}

static double my_xstrtod(const char *arg)
{
	double result;
	multiconvert(arg, &result, conv_strtod);
	return result;
}

static void print_esc_string(char *str)
{
	for (; *str; str++) {
		if (*str == '\\') {
			str++;
			bb_putchar(bb_process_escape_sequence((const char **)&str));
		} else {
			bb_putchar(*str);
		}

	}
}

static void print_direc(char *start, size_t length, int field_width, int precision,
		const char *argument)
{
	char *p;		/* Null-terminated copy of % directive. */

	p = xmalloc((unsigned) (length + 1));
	strncpy(p, start, length);
	p[length] = 0;

	switch (p[length - 1]) {
	case 'd':
	case 'i':
		if (field_width < 0) {
			if (precision < 0)
				printf(p, my_xstrtol(argument));
			else
				printf(p, precision, my_xstrtol(argument));
		} else {
			if (precision < 0)
				printf(p, field_width, my_xstrtol(argument));
			else
				printf(p, field_width, precision, my_xstrtol(argument));
		}
		break;
	case 'o':
	case 'u':
	case 'x':
	case 'X':
		if (field_width < 0) {
			if (precision < 0)
				printf(p, my_xstrtoul(argument));
			else
				printf(p, precision, my_xstrtoul(argument));
		} else {
			if (precision < 0)
				printf(p, field_width, my_xstrtoul(argument));
			else
				printf(p, field_width, precision, my_xstrtoul(argument));
		}
		break;
	case 'f':
	case 'e':
	case 'E':
	case 'g':
	case 'G':
		if (field_width < 0) {
			if (precision < 0)
				printf(p, my_xstrtod(argument));
			else
				printf(p, precision, my_xstrtod(argument));
		} else {
			if (precision < 0)
				printf(p, field_width, my_xstrtod(argument));
			else
				printf(p, field_width, precision, my_xstrtod(argument));
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

/* Print the text in FORMAT, using ARGV (with ARGC elements) for
   arguments to any '%' directives.
   Return the number of elements of ARGV used.  */

static int print_formatted(char *format, int argc, char **argv)
{
	int save_argc = argc;   /* Preserve original value.  */
	char *f;                /* Pointer into 'format'.  */
	char *direc_start;      /* Start of % directive.  */
	size_t direc_length;    /* Length of % directive.  */
	int field_width;        /* Arg to first '*', or -1 if none.  */
	int precision;          /* Arg to second '*', or -1 if none.  */

	for (f = format; *f; ++f) {
		switch (*f) {
		case '%':
			direc_start = f++;
			direc_length = 1;
			field_width = precision = -1;
			if (*f == '%') {
				bb_putchar('%');
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
					field_width = my_xstrtoul(*argv);
					++argv;
					--argc;
				} else
					field_width = 0;
			} else {
				while (isdigit(*f)) {
					++f;
					++direc_length;
				}
			}
			if (*f == '.') {
				++f;
				++direc_length;
				if (*f == '*') {
					++f;
					++direc_length;
					if (argc > 0) {
						precision = my_xstrtoul(*argv);
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
			bb_putchar(bb_process_escape_sequence((const char **)&f));
			f--;
			break;
		default:
			bb_putchar(*f);
		}
	}

	return save_argc - argc;
}

int printf_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int printf_main(int argc, char **argv)
{
	char *format;
	int args_used;

	if (argc <= 1 || argv[1][0] == '-') {
		bb_show_usage();
	}

	format = argv[1];
	argc -= 2;
	argv += 2;

	do {
		args_used = print_formatted(format, argc, argv);
		argc -= args_used;
		argv += args_used;
	} while (args_used > 0 && argc > 0);

/*	if (argc > 0)
		fprintf(stderr, "excess args ignored");
*/

	return EXIT_SUCCESS;
}
