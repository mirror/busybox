/* vi: set sw=4 ts=4: */
/*
 * Mini xargs implementation for busybox
 * Options are supported: "-prtx -n max_arg -s max_chars -e[ouf_str]"
 *
 * (C) 2002,2003 by Vladimir Oleynik <dzo@simtreas.ru>
 *
 * Special thanks
 * - Mark Whitley and Glenn McGrath for stimulus to rewrite :)
 * - Mike Rendell <michael@cs.mun.ca>
 * and David MacKenzie <djm@gnu.ai.mit.edu>.
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 *
 * xargs is described in the Single Unix Specification v3 at
 * http://www.opengroup.org/onlinepubs/007904975/utilities/xargs.html
 *
 */

#include "libbb.h"

/* This is a NOEXEC applet. Be very careful! */


/* COMPAT:  SYSV version defaults size (and has a max value of) to 470.
   We try to make it as large as possible. */
#if !defined(ARG_MAX) && defined(_SC_ARG_MAX)
#define ARG_MAX sysconf (_SC_ARG_MAX)
#endif
#ifndef ARG_MAX
#define ARG_MAX 470
#endif


#ifdef TEST
# ifndef ENABLE_FEATURE_XARGS_SUPPORT_CONFIRMATION
#  define ENABLE_FEATURE_XARGS_SUPPORT_CONFIRMATION 1
# endif
# ifndef ENABLE_FEATURE_XARGS_SUPPORT_QUOTES
#  define ENABLE_FEATURE_XARGS_SUPPORT_QUOTES 1
# endif
# ifndef ENABLE_FEATURE_XARGS_SUPPORT_TERMOPT
#  define ENABLE_FEATURE_XARGS_SUPPORT_TERMOPT 1
# endif
# ifndef ENABLE_FEATURE_XARGS_SUPPORT_ZERO_TERM
#  define ENABLE_FEATURE_XARGS_SUPPORT_ZERO_TERM 1
# endif
#endif

/*
   This function has special algorithm.
   Don't use fork and include to main!
*/
static int xargs_exec(char **args)
{
	int status;

	status = spawn_and_wait(args);
	if (status < 0) {
		bb_simple_perror_msg(args[0]);
		return errno == ENOENT ? 127 : 126;
	}
	if (status == 255) {
		bb_error_msg("%s: exited with status 255; aborting", args[0]);
		return 124;
	}
/* Huh? I think we won't see this, ever. We don't wait with WUNTRACED!
	if (WIFSTOPPED(status)) {
		bb_error_msg("%s: stopped by signal %d",
			args[0], WSTOPSIG(status));
		return 125;
	}
*/
	if (status >= 1000) {
		bb_error_msg("%s: terminated by signal %d",
			args[0], status - 1000);
		return 125;
	}
	if (status)
		return 123;
	return 0;
}


typedef struct xlist_t {
	struct xlist_t *link;
	size_t length;
	char xstr[1];
} xlist_t;

static smallint eof_stdin_detected;

#define ISBLANK(c) ((c) == ' ' || (c) == '\t')
#define ISSPACE(c) (ISBLANK(c) || (c) == '\n' || (c) == '\r' \
		    || (c) == '\f' || (c) == '\v')

#if ENABLE_FEATURE_XARGS_SUPPORT_QUOTES
static xlist_t *process_stdin(xlist_t *list_arg,
	const char *eof_str, size_t mc, char *buf)
{
#define NORM      0
#define QUOTE     1
#define BACKSLASH 2
#define SPACE     4

	char *s = NULL;         /* start word */
	char *p = NULL;         /* pointer to end word */
	char q = '\0';          /* quote char */
	char state = NORM;
	char eof_str_detected = 0;
	size_t line_l = 0;      /* size loaded args line */
	int c;                  /* current char */
	xlist_t *cur;
	xlist_t *prev;

	prev = cur = list_arg;
	while (1) {
		if (!cur) break;
		prev = cur;
		line_l += cur->length;
		cur = cur->link;
	}

	while (!eof_stdin_detected) {
		c = getchar();
		if (c == EOF) {
			eof_stdin_detected = 1;
			if (s)
				goto unexpected_eof;
			break;
		}
		if (eof_str_detected)
			continue;
		if (state == BACKSLASH) {
			state = NORM;
			goto set;
		} else if (state == QUOTE) {
			if (c != q)
				goto set;
			q = '\0';
			state = NORM;
		} else { /* if (state == NORM) */
			if (ISSPACE(c)) {
				if (s) {
 unexpected_eof:
					state = SPACE;
					c = '\0';
					goto set;
				}
			} else {
				if (s == NULL)
					s = p = buf;
				if (c == '\\') {
					state = BACKSLASH;
				} else if (c == '\'' || c == '"') {
					q = c;
					state = QUOTE;
				} else {
 set:
					if ((size_t)(p - buf) >= mc)
						bb_error_msg_and_die("argument line too long");
					*p++ = c;
				}
			}
		}
		if (state == SPACE) {   /* word's delimiter or EOF detected */
			if (q) {
				bb_error_msg_and_die("unmatched %s quote",
					q == '\'' ? "single" : "double");
			}
			/* word loaded */
			if (eof_str) {
				eof_str_detected = (strcmp(s, eof_str) == 0);
			}
			if (!eof_str_detected) {
				size_t length = (p - buf);
				/* Dont xzalloc - it can be quite big */
				cur = xmalloc(offsetof(xlist_t, xstr) + length);
				cur->link = NULL;
				cur->length = length;
				memcpy(cur->xstr, s, length);
				if (prev == NULL) {
					list_arg = cur;
				} else {
					prev->link = cur;
				}
				prev = cur;
				line_l += length;
				if (line_l > mc) {
					/* stop memory usage :-) */
					break;
				}
			}
			s = NULL;
			state = NORM;
		}
	}
	return list_arg;
}
#else
/* The variant does not support single quotes, double quotes or backslash */
static xlist_t *process_stdin(xlist_t *list_arg,
		const char *eof_str, size_t mc, char *buf)
{

	int c;                  /* current char */
	char eof_str_detected = 0;
	char *s = NULL;         /* start word */
	char *p = NULL;         /* pointer to end word */
	size_t line_l = 0;      /* size loaded args line */
	xlist_t *cur;
	xlist_t *prev;

	prev = cur = list_arg;
	while (1) {
		if (!cur) break;
		prev = cur;
		line_l += cur->length;
		cur = cur->link;
	}

	while (!eof_stdin_detected) {
		c = getchar();
		if (c == EOF) {
			eof_stdin_detected = 1;
		}
		if (eof_str_detected)
			continue;
		if (c == EOF || ISSPACE(c)) {
			if (s == NULL)
				continue;
			c = EOF;
		}
		if (s == NULL)
			s = p = buf;
		if ((size_t)(p - buf) >= mc)
			bb_error_msg_and_die("argument line too long");
		*p++ = (c == EOF ? '\0' : c);
		if (c == EOF) { /* word's delimiter or EOF detected */
			/* word loaded */
			if (eof_str) {
				eof_str_detected = (strcmp(s, eof_str) == 0);
			}
			if (!eof_str_detected) {
				size_t length = (p - buf);
				/* Dont xzalloc - it can be quite big */
				cur = xmalloc(offsetof(xlist_t, xstr) + length);
				cur->link = NULL;
				cur->length = length;
				memcpy(cur->xstr, s, length);
				if (prev == NULL) {
					list_arg = cur;
				} else {
					prev->link = cur;
				}
				prev = cur;
				line_l += length;
				if (line_l > mc) {
					/* stop memory usage :-) */
					break;
				}
				s = NULL;
			}
		}
	}
	return list_arg;
}
#endif /* FEATURE_XARGS_SUPPORT_QUOTES */


#if ENABLE_FEATURE_XARGS_SUPPORT_CONFIRMATION
/* Prompt the user for a response, and
   if the user responds affirmatively, return true;
   otherwise, return false. Uses "/dev/tty", not stdin. */
static int xargs_ask_confirmation(void)
{
	FILE *tty_stream;
	int c, savec;

	tty_stream = xfopen_for_read(CURRENT_TTY);
	fputs(" ?...", stderr);
	fflush_all();
	c = savec = getc(tty_stream);
	while (c != EOF && c != '\n')
		c = getc(tty_stream);
	fclose(tty_stream);
	return (savec == 'y' || savec == 'Y');
}
#else
# define xargs_ask_confirmation() 1
#endif /* FEATURE_XARGS_SUPPORT_CONFIRMATION */

#if ENABLE_FEATURE_XARGS_SUPPORT_ZERO_TERM
static xlist_t *process0_stdin(xlist_t *list_arg,
		const char *eof_str UNUSED_PARAM, size_t mc, char *buf)
{
	int c;                  /* current char */
	char *s = NULL;         /* start word */
	char *p = NULL;         /* pointer to end word */
	size_t line_l = 0;      /* size loaded args line */
	xlist_t *cur;
	xlist_t *prev;

	prev = cur = list_arg;
	while (1) {
		if (!cur) break;
		prev = cur;
		line_l += cur->length;
		cur = cur->link;
	}

	while (!eof_stdin_detected) {
		c = getchar();
		if (c == EOF) {
			eof_stdin_detected = 1;
			if (s == NULL)
				break;
			c = '\0';
		}
		if (s == NULL)
			s = p = buf;
		if ((size_t)(p - buf) >= mc)
			bb_error_msg_and_die("argument line too long");
		*p++ = c;
		if (c == '\0') {   /* word's delimiter or EOF detected */
			/* word loaded */
			size_t length = (p - buf);
			/* Dont xzalloc - it can be quite big */
			cur = xmalloc(offsetof(xlist_t, xstr) + length);
			cur->link = NULL;
			cur->length = length;
			memcpy(cur->xstr, s, length);
			if (prev == NULL) {
				list_arg = cur;
			} else {
				prev->link = cur;
			}
			prev = cur;
			line_l += length;
			if (line_l > mc) {
				/* stop memory usage :-) */
				break;
			}
			s = NULL;
		}
	}
	return list_arg;
}
#endif /* FEATURE_XARGS_SUPPORT_ZERO_TERM */

/* Correct regardless of combination of CONFIG_xxx */
enum {
	OPTBIT_VERBOSE = 0,
	OPTBIT_NO_EMPTY,
	OPTBIT_UPTO_NUMBER,
	OPTBIT_UPTO_SIZE,
	OPTBIT_EOF_STRING,
	OPTBIT_EOF_STRING1,
	IF_FEATURE_XARGS_SUPPORT_CONFIRMATION(OPTBIT_INTERACTIVE,)
	IF_FEATURE_XARGS_SUPPORT_TERMOPT(     OPTBIT_TERMINATE  ,)
	IF_FEATURE_XARGS_SUPPORT_ZERO_TERM(   OPTBIT_ZEROTERM   ,)

	OPT_VERBOSE     = 1 << OPTBIT_VERBOSE    ,
	OPT_NO_EMPTY    = 1 << OPTBIT_NO_EMPTY   ,
	OPT_UPTO_NUMBER = 1 << OPTBIT_UPTO_NUMBER,
	OPT_UPTO_SIZE   = 1 << OPTBIT_UPTO_SIZE  ,
	OPT_EOF_STRING  = 1 << OPTBIT_EOF_STRING , /* GNU: -e[<param>] */
	OPT_EOF_STRING1 = 1 << OPTBIT_EOF_STRING1, /* SUS: -E<param> */
	OPT_INTERACTIVE = IF_FEATURE_XARGS_SUPPORT_CONFIRMATION((1 << OPTBIT_INTERACTIVE)) + 0,
	OPT_TERMINATE   = IF_FEATURE_XARGS_SUPPORT_TERMOPT(     (1 << OPTBIT_TERMINATE  )) + 0,
	OPT_ZEROTERM    = IF_FEATURE_XARGS_SUPPORT_ZERO_TERM(   (1 << OPTBIT_ZEROTERM   )) + 0,
};
#define OPTION_STR "+trn:s:e::E:" \
	IF_FEATURE_XARGS_SUPPORT_CONFIRMATION("p") \
	IF_FEATURE_XARGS_SUPPORT_TERMOPT(     "x") \
	IF_FEATURE_XARGS_SUPPORT_ZERO_TERM(   "0")

int xargs_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int xargs_main(int argc, char **argv)
{
	char **args;
	int i, n;
	xlist_t *list = NULL;
	xlist_t *cur;
	int child_error = 0;
	char *max_args, *max_chars;
	int n_max_arg;
	size_t n_chars = 0;
	long orig_arg_max;
	const char *eof_str = NULL;
	unsigned opt;
	size_t n_max_chars;
#if ENABLE_FEATURE_XARGS_SUPPORT_ZERO_TERM
	xlist_t* (*read_args)(xlist_t*, const char*, size_t, char*) = process_stdin;
#else
#define read_args process_stdin
#endif

	opt = getopt32(argv, OPTION_STR, &max_args, &max_chars, &eof_str, &eof_str);

	/* -E ""? You may wonder why not just omit -E?
	 * This is used for portability:
	 * old xargs was using "_" as default for -E / -e */
	if ((opt & OPT_EOF_STRING1) && eof_str[0] == '\0')
		eof_str = NULL;

	if (opt & OPT_ZEROTERM)
		IF_FEATURE_XARGS_SUPPORT_ZERO_TERM(read_args = process0_stdin);

	argv += optind;
	argc -= optind;
	if (!argc) {
		/* default behavior is to echo all the filenames */
		*argv = (char*)"echo";
		argc++;
	}

	orig_arg_max = ARG_MAX;
	if (orig_arg_max == -1)
		orig_arg_max = LONG_MAX;
	orig_arg_max -= 2048;   /* POSIX.2 requires subtracting 2048 */

	if (opt & OPT_UPTO_SIZE) {
		n_max_chars = xatoul_range(max_chars, 1, orig_arg_max);
		for (i = 0; i < argc; i++) {
			n_chars += strlen(*argv) + 1;
		}
		if (n_max_chars < n_chars) {
			bb_error_msg_and_die("can't fit single argument within argument list size limit");
		}
		n_max_chars -= n_chars;
	} else {
		/* Sanity check for systems with huge ARG_MAX defines (e.g., Suns which
		   have it at 1 meg).  Things will work fine with a large ARG_MAX but it
		   will probably hurt the system more than it needs to; an array of this
		   size is allocated.  */
		if (orig_arg_max > 20 * 1024)
			orig_arg_max = 20 * 1024;
		n_max_chars = orig_arg_max;
	}
	max_chars = xmalloc(n_max_chars);

	if (opt & OPT_UPTO_NUMBER) {
		n_max_arg = xatoul_range(max_args, 1, INT_MAX);
	} else {
		n_max_arg = n_max_chars;
	}

	while ((list = read_args(list, eof_str, n_max_chars, max_chars)) != NULL ||
		!(opt & OPT_NO_EMPTY))
	{
		opt |= OPT_NO_EMPTY;
		n = 0;
		n_chars = 0;
#if ENABLE_FEATURE_XARGS_SUPPORT_TERMOPT
		for (cur = list; cur;) {
			n_chars += cur->length;
			n++;
			cur = cur->link;
			if (n_chars > n_max_chars || (n == n_max_arg && cur)) {
				if (opt & OPT_TERMINATE)
					bb_error_msg_and_die("argument list too long");
				break;
			}
		}
#else
		for (cur = list; cur; cur = cur->link) {
			n_chars += cur->length;
			n++;
			if (n_chars > n_max_chars || n == n_max_arg) {
				break;
			}
		}
#endif /* FEATURE_XARGS_SUPPORT_TERMOPT */

		/* allocate pointers for execvp:
		   argc*arg, n*arg from stdin, NULL */
		args = xzalloc((n + argc + 1) * sizeof(char *));

		/* store the command to be executed
		   (taken from the command line) */
		for (i = 0; i < argc; i++)
			args[i] = argv[i];
		/* (taken from stdin) */
		for (cur = list; n; cur = cur->link) {
			args[i++] = cur->xstr;
			n--;
		}

		if (opt & (OPT_INTERACTIVE | OPT_VERBOSE)) {
			for (i = 0; args[i]; i++) {
				if (i)
					fputc(' ', stderr);
				fputs(args[i], stderr);
			}
			if (!(opt & OPT_INTERACTIVE))
				fputc('\n', stderr);
		}
		if (!(opt & OPT_INTERACTIVE) || xargs_ask_confirmation()) {
			child_error = xargs_exec(args);
		}

		/* clean up */
		for (i = argc; args[i]; i++) {
			cur = list;
			list = list->link;
			free(cur);
		}
		free(args);
		if (child_error > 0 && child_error != 123) {
			break;
		}
	} /* while */
	if (ENABLE_FEATURE_CLEAN_UP)
		free(max_chars);
	return child_error;
}


#ifdef TEST

const char *applet_name = "debug stuff usage";

void bb_show_usage(void)
{
	fprintf(stderr, "Usage: %s [-p] [-r] [-t] -[x] [-n max_arg] [-s max_chars]\n",
		applet_name);
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	return xargs_main(argc, argv);
}
#endif /* TEST */
