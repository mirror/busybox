/*
 * Mini xargs implementation for busybox
 * Options are supported: "-prtx -n max_arg -s max_chars -e[ouf_str]"
 *
 * (C) 2002,2003 by Vladimir Oleynik <dzo@simtreas.ru>
 *
 * Special thanks
 * - Mark Whitley and Glenn McGrath for stimul to rewrote :)
 * - Mike Rendell <michael@cs.mun.ca>
 * and David MacKenzie <djm@gnu.ai.mit.edu>.
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
 * xargs is described in the Single Unix Specification v3 at
 * http://www.opengroup.org/onlinepubs/007904975/utilities/xargs.html
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "busybox.h"

/* COMPAT:  SYSV version defaults size (and has a max value of) to 470.
   We try to make it as large as possible. */
#if !defined(ARG_MAX) && defined(_SC_ARG_MAX)
#define ARG_MAX sysconf (_SC_ARG_MAX)
#endif
#ifndef ARG_MAX
#define ARG_MAX 470
#endif


#ifdef TEST
# ifndef CONFIG_FEATURE_XARGS_SUPPORT_CONFIRMATION
#  define CONFIG_FEATURE_XARGS_SUPPORT_CONFIRMATION
# endif
# ifndef CONFIG_FEATURE_XARGS_SUPPORT_QUOTES
#  define CONFIG_FEATURE_XARGS_SUPPORT_QUOTES
# endif
# ifndef CONFIG_FEATURE_XARGS_SUPPORT_TERMOPT
#  define CONFIG_FEATURE_XARGS_SUPPORT_TERMOPT
# endif
# ifndef CONFIG_FEATURE_XARGS_SUPPORT_ZERO_TERM
#  define CONFIG_FEATURE_XARGS_SUPPORT_ZERO_TERM
# endif
#endif

/*
   This function have special algorithm.
   Don`t use fork and include to main!
*/
static int xargs_exec(char *const *args)
{
	pid_t p;
	volatile int exec_errno = 0;	/* shared vfork stack */

	if ((p = vfork()) >= 0) {
		if (p == 0) {
			/* vfork -- child */
			execvp(args[0], args);
			exec_errno = errno;	/* set error to shared stack */
			_exit(1);
		} else {
			/* vfork -- parent */
			int status;

			while (wait(&status) == (pid_t) - 1)
				if (errno != EINTR)
					break;
			if (exec_errno) {
				errno = exec_errno;
				bb_perror_msg("%s", args[0]);
				return exec_errno == ENOENT ? 127 : 126;
			} else {
				if (WEXITSTATUS(status) == 255) {
					bb_error_msg("%s: exited with status 255; aborting", args[0]);
					return 124;
				}
				if (WIFSTOPPED(status)) {
					bb_error_msg("%s: stopped by signal %d", 
						args[0], WSTOPSIG(status));
					return 125;
				}
				if (WIFSIGNALED(status)) {
					bb_error_msg("%s: terminated by signal %d", 
						args[0], WTERMSIG(status));
					return 125;
				}
				if (WEXITSTATUS(status) != 0)
					return 123;
				return 0;
			}
		}
	} else {
		bb_perror_msg_and_die("vfork");
	}
}


typedef struct xlist_s {
	char *data;
	size_t lenght;
	struct xlist_s *link;
} xlist_t;

static int eof_stdin_detected;

#define ISBLANK(c) ((c) == ' ' || (c) == '\t')
#define ISSPACE(c) (ISBLANK (c) || (c) == '\n' || (c) == '\r' \
		    || (c) == '\f' || (c) == '\v')

#ifdef CONFIG_FEATURE_XARGS_SUPPORT_QUOTES
static xlist_t *process_stdin(xlist_t * list_arg, 
	const char *eof_str, size_t mc, char *buf)
{
#define NORM      0
#define QUOTE     1
#define BACKSLASH 2
#define SPACE     4

	char *s = NULL;		/* start word */
	char *p = NULL;		/* pointer to end word */
	char q = 0;		/* quote char */
	char state = NORM;
	char eof_str_detected = 0;
	size_t line_l = 0;	/* size loaded args line */
	int c;			/* current char */
	xlist_t *cur;
	xlist_t *prev;

	for (prev = cur = list_arg; cur; cur = cur->link) {
		line_l += cur->lenght;	/* previous allocated */
		if (prev != cur)
			prev = prev->link;
	}

	while (!eof_stdin_detected) {
		c = getchar();
		if (c == EOF) {
			eof_stdin_detected++;
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
			if (c == q) {
				q = 0;
				state = NORM;
			} else {
				goto set;
			}
		} else { /* if(state == NORM) */

			if (ISSPACE(c)) {
				if (s) {
unexpected_eof:
					state = SPACE;
					c = 0;
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
					if ((p - buf) >= mc)
						bb_error_msg_and_die("argument line too long");
					*p++ = c;
				}
			}
		}
		if (state == SPACE) {	/* word's delimiter or EOF detected */
			if (q) {
				bb_error_msg_and_die("unmatched %s quote", 
					q == '\'' ? "single" : "double");
			}
			/* word loaded */
			if (eof_str) {
				eof_str_detected = strcmp(s, eof_str) == 0;
			}
			if (!eof_str_detected) {
				size_t lenght = (p - buf);

				cur = xmalloc(sizeof(xlist_t) + lenght);
				cur->data = memcpy(cur + 1, s, lenght);
				cur->lenght = lenght;
				cur->link = NULL;
				if (prev == NULL) {
					list_arg = cur;
				} else {
					prev->link = cur;
				}
				prev = cur;
				line_l += lenght;
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
static xlist_t *process_stdin(xlist_t * list_arg, 
	const char *eof_str, size_t mc, char *buf)
{

	int c;			/* current char */
	int eof_str_detected = 0;
	char *s = NULL;		/* start word */
	char *p = NULL;		/* pointer to end word */
	size_t line_l = 0;	/* size loaded args line */
	xlist_t *cur;
	xlist_t *prev;

	for (prev = cur = list_arg; cur; cur = cur->link) {
		line_l += cur->lenght;	/* previous allocated */
		if (prev != cur)
			prev = prev->link;
	}

	while (!eof_stdin_detected) {
		c = getchar();
		if (c == EOF) {
			eof_stdin_detected++;
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
		if ((p - buf) >= mc)
			bb_error_msg_and_die("argument line too long");
		*p++ = c == EOF ? 0 : c;
		if (c == EOF) {	/* word's delimiter or EOF detected */
			/* word loaded */
			if (eof_str) {
				eof_str_detected = strcmp(s, eof_str) == 0;
			}
			if (!eof_str_detected) {
				size_t lenght = (p - buf);

				cur = xmalloc(sizeof(xlist_t) + lenght);
				cur->data = memcpy(cur + 1, s, lenght);
				cur->lenght = lenght;
				cur->link = NULL;
				if (prev == NULL) {
					list_arg = cur;
				} else {
					prev->link = cur;
				}
				prev = cur;
				line_l += lenght;
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
#endif /* CONFIG_FEATURE_XARGS_SUPPORT_QUOTES */


#ifdef CONFIG_FEATURE_XARGS_SUPPORT_CONFIRMATION
/* Prompt the user for a response, and
   if the user responds affirmatively, return true;
   otherwise, return false. Used "/dev/tty", not stdin. */
static int xargs_ask_confirmation(void)
{
	static FILE *tty_stream;
	int c, savec;

	if (!tty_stream) {
		tty_stream = fopen("/dev/tty", "r");
		if (!tty_stream)
			bb_perror_msg_and_die("/dev/tty");
		/* pranoidal security by vodz */
		fcntl(fileno(tty_stream), F_SETFD, FD_CLOEXEC);
	}
	fputs(" ?...", stderr);
	fflush(stderr);
	c = savec = getc(tty_stream);
	while (c != EOF && c != '\n')
		c = getc(tty_stream);
	if (savec == 'y' || savec == 'Y')
		return 1;
	return 0;
}

# define OPT_INC_P 1
#else
# define OPT_INC_P 0
# define xargs_ask_confirmation() 1
#endif /* CONFIG_FEATURE_XARGS_SUPPORT_CONFIRMATION */

#ifdef CONFIG_FEATURE_XARGS_SUPPORT_TERMOPT
# define OPT_INC_X 1
#else
# define OPT_INC_X 0
#endif

#ifdef CONFIG_FEATURE_XARGS_SUPPORT_ZERO_TERM
static xlist_t *process0_stdin(xlist_t * list_arg, const char *eof_str,
							   size_t mc, char *buf)
{
	int c;			/* current char */
	char *s = NULL;		/* start word */
	char *p = NULL;		/* pointer to end word */
	size_t line_l = 0;	/* size loaded args line */
	xlist_t *cur;
	xlist_t *prev;

	for (prev = cur = list_arg; cur; cur = cur->link) {
		line_l += cur->lenght;	/* previous allocated */
		if (prev != cur)
			prev = prev->link;
	}

	while (!eof_stdin_detected) {
		c = getchar();
		if (c == EOF) {
			eof_stdin_detected++;
			if (s == NULL)
				break;
			c = 0;
		}
		if (s == NULL)
			s = p = buf;
		if ((p - buf) >= mc)
			bb_error_msg_and_die("argument line too long");
		*p++ = c;
		if (c == 0) {	/* word's delimiter or EOF detected */
			/* word loaded */
			size_t lenght = (p - buf);

			cur = xmalloc(sizeof(xlist_t) + lenght);
			cur->data = memcpy(cur + 1, s, lenght);
			cur->lenght = lenght;
			cur->link = NULL;
			if (prev == NULL) {
				list_arg = cur;
			} else {
				prev->link = cur;
			}
			prev = cur;
			line_l += lenght;
			if (line_l > mc) {
				/* stop memory usage :-) */
				break;
			}
			s = NULL;
		}
	}
	return list_arg;
}

# define READ_ARGS(l, e, nmc, mc) (*read_args)(l, e, nmc, mc)
# define OPT_INC_0 1	/* future use */
#else
# define OPT_INC_0 0	/* future use */
# define READ_ARGS(l, e, nmc, mc) process_stdin(l, e, nmc, mc)
#endif /* CONFIG_FEATURE_XARGS_SUPPORT_ZERO_TERM */


#define OPT_VERBOSE     (1<<0)
#define OPT_NO_EMPTY    (1<<1)
#define OPT_UPTO_NUMBER (1<<2)
#define OPT_UPTO_SIZE   (1<<3)
#define OPT_EOF_STRING  (1<<4)
#ifdef CONFIG_FEATURE_XARGS_SUPPORT_CONFIRMATION
#define OPT_INTERACTIVE (1<<5)
#else
#define OPT_INTERACTIVE (0)	/* require for algorithm &| */
#endif
#define OPT_TERMINATE   (1<<(5+OPT_INC_P))
#define OPT_ZEROTERM    (1<<(5+OPT_INC_P+OPT_INC_X))
/* next future
#define OPT_NEXT_OTHER  (1<<(5+OPT_INC_P+OPT_INC_X+OPT_INC_0))
*/

int xargs_main(int argc, char **argv)
{
	char **args;
	int i, a, n;
	xlist_t *list = NULL;
	xlist_t *cur;
	int child_error = 0;
	char *max_args, *max_chars;
	int n_max_arg;
	size_t n_chars = 0;
	long orig_arg_max;
	const char *eof_str = "_";
	unsigned long opt;
	size_t n_max_chars;

#ifdef CONFIG_FEATURE_XARGS_SUPPORT_ZERO_TERM
	xlist_t *(*read_args) (xlist_t *, const char *, size_t, char *) = process_stdin;
#endif

#ifdef CONFIG_FEATURE_XARGS_SUPPORT_CONFIRMATION
	bb_opt_complementaly = "pt";
#endif

	opt = bb_getopt_ulflags(argc, argv, "+trn:s:e::"
#ifdef CONFIG_FEATURE_XARGS_SUPPORT_CONFIRMATION
	"p"
#endif
#ifdef CONFIG_FEATURE_XARGS_SUPPORT_TERMOPT
	"x"
#endif
#ifdef CONFIG_FEATURE_XARGS_SUPPORT_ZERO_TERM
	"0"
#endif
	,&max_args, &max_chars, &eof_str);

	a = argc - optind;
	argv += optind;
	if (a == 0) {
		/* default behavior is to echo all the filenames */
		*argv = "echo";
		a++;
	}

	orig_arg_max = ARG_MAX;
	if (orig_arg_max == -1)
		orig_arg_max = LONG_MAX;
	orig_arg_max -= 2048;	/* POSIX.2 requires subtracting 2048.  */
	if ((opt & OPT_UPTO_SIZE)) {
		n_max_chars = bb_xgetularg10_bnd(max_chars, 1, orig_arg_max);
		for (i = 0; i < a; i++) {
			n_chars += strlen(*argv) + 1;
		}
		if (n_max_chars < n_chars) {
			bb_error_msg_and_die("can not fit single argument within argument list size limit");
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

	if ((opt & OPT_UPTO_NUMBER)) {
		n_max_arg = bb_xgetularg10_bnd(max_args, 1, INT_MAX);
	} else {
		n_max_arg = n_max_chars;
	}

#ifdef CONFIG_FEATURE_XARGS_SUPPORT_ZERO_TERM
	if (opt & OPT_ZEROTERM)
		read_args = process0_stdin;
#endif

	while ((list = READ_ARGS(list, eof_str, n_max_chars, max_chars)) != NULL || 
		(opt & OPT_NO_EMPTY) == 0) 
	{
		opt |= OPT_NO_EMPTY;
		n = 0;
		n_chars = 0;
#ifdef CONFIG_FEATURE_XARGS_SUPPORT_TERMOPT
		for (cur = list; cur;) {
			n_chars += cur->lenght;
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
			n_chars += cur->lenght;
			n++;
			if (n_chars > n_max_chars || n == n_max_arg) {
				break;
			}
		}
#endif /* CONFIG_FEATURE_XARGS_SUPPORT_TERMOPT */

		/* allocating pointers for execvp:
		   a*arg, n*arg from stdin, NULL */
		args = xcalloc(n + a + 1, sizeof(char *));

		/* Store the command to be executed
		   (taken from the command line) */
		for (i = 0; i < a; i++)
			args[i] = argv[i];
		/* (taken from stdin) */
		for (cur = list; n; cur = cur->link) {
			args[i++] = cur->data;
			n--;
		}

		if ((opt & (OPT_INTERACTIVE | OPT_VERBOSE))) {
			for (i = 0; args[i]; i++) {
				if (i)
					fputc(' ', stderr);
				fputs(args[i], stderr);
			}
			if ((opt & OPT_INTERACTIVE) == 0)
				fputc('\n', stderr);
		}
		if ((opt & OPT_INTERACTIVE) == 0 || xargs_ask_confirmation() != 0) {
			child_error = xargs_exec(args);
		}

		/* clean up */
		for (i = a; args[i]; i++) {
			cur = list;
			list = list->link;
			free(cur);
		}
		free(args);
		if (child_error > 0 && child_error != 123) {
			break;
		}
	}
#ifdef CONFIG_FEATURE_CLEAN_UP
	free(max_chars);
#endif
	return child_error;
}


#ifdef TEST

const char *bb_applet_name = "debug stuff usage";

void bb_show_usage(void)
{
	fprintf(stderr, "Usage: %s [-p] [-r] [-t] -[x] [-n max_arg] [-s max_chars]\n", 
		bb_applet_name);
	exit(1);
}

int main(int argc, char **argv)
{
	return xargs_main(argc, argv);
}
#endif /* TEST */
