/* vi: set sw=4 ts=4: */
/*
 * sh.c -- a prototype Bourne shell grammar parser
 *      Intended to follow the original Thompson and Ritchie
 *      "small and simple is beautiful" philosophy, which
 *      incidentally is a good match to today's BusyBox.
 *
 * Copyright (C) 2000,2001  Larry Doolittle  <larry@doolittle.boa.org>
 *
 * Credits:
 *      The parser routines proper are all original material, first
 *      written Dec 2000 and Jan 2001 by Larry Doolittle.
 *      The execution engine, the builtins, and much of the underlying
 *      support has been adapted from busybox-0.49pre's lash,
 *      which is Copyright (C) 2000 by Lineo, Inc., and
 *      written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>.
 *      That, in turn, is based in part on ladsh.c, by Michael K. Johnson and
 *      Erik W. Troan, which they placed in the public domain.  I don't know
 *      how much of the Johnson/Troan code has survived the repeated rewrites.
 * Other credits:
 *      simple_itoa() was lifted from boa-0.93.15
 *      b_addchr() derived from similar w_addchar function in glibc-2.2
 *      setup_redirect(), redirect_opt_num(), and big chunks of main()
 *        and many builtins derived from contributions by Erik Andersen
 *      miscellaneous bugfixes from Matt Kraai
 *
 * There are two big (and related) architecture differences between
 * this parser and the lash parser.  One is that this version is
 * actually designed from the ground up to understand nearly all
 * of the Bourne grammar.  The second, consequential change is that
 * the parser and input reader have been turned inside out.  Now,
 * the parser is in control, and asks for input as needed.  The old
 * way had the input reader in control, and it asked for parsing to
 * take place as needed.  The new way makes it much easier to properly
 * handle the recursion implicit in the various substitutions, especially
 * across continuation lines.
 *
 * Bash grammar not implemented: (how many of these were in original sh?)
 *      $@ (those sure look like weird quoting rules)
 *      $_
 *      ! negation operator for pipes
 *      &> and >& redirection of stdout+stderr
 *      Brace Expansion
 *      Tilde Expansion
 *      fancy forms of Parameter Expansion
 *      Arithmetic Expansion
 *      <(list) and >(list) Process Substitution
 *      reserved words: case, esac, function
 *      Here Documents ( << word )
 *      Functions
 * Major bugs:
 *      job handling woefully incomplete and buggy
 *      reserved word execution woefully incomplete and buggy
 * to-do:
 *      port selected bugfixes from post-0.49 busybox lash
 *      finish implementing reserved words
 *      handle children going into background
 *      clean up recognition of null pipes
 *      have builtin_exec set flag to avoid restore_redirects
 *      figure out if "echo foo}" is fixable
 *      check setting of global_argc and global_argv
 *      control-C handling, probably with longjmp
 *      VAR=value prefix for simple commands
 *      follow IFS rules more precisely, including update semantics
 *      write builtin_eval, builtin_ulimit, builtin_umask
 *      figure out what to do with backslash-newline
 *      explain why we use signal instead of sigaction
 *      propagate syntax errors, die on resource errors?
 *      continuation lines, both explicit and implicit - done?
 *      memory leak finding and plugging - done?
 *      more testing, especially quoting rules and redirection
 *      maybe change map[] to use 2-bit entries
 *      (eventually) remove all the printf's
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
 */
#include <ctype.h>     /* isalpha, isdigit */
#include <unistd.h>    /* getpid */
#include <stdlib.h>    /* getenv, atoi */
#include <string.h>    /* strchr */
#include <stdio.h>     /* popen etc. */
#include <glob.h>      /* glob, of course */
#include <stdarg.h>    /* va_list */
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>    /* should be pretty obvious */

#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>

/* #include <dmalloc.h> */
#define DEBUG_SHELL

#ifdef BB_VER
#include "busybox.h"
#include "cmdedit.h"
#else
#define applet_name "hush"
#include "standalone.h"
#define shell_main main
#define BB_FEATURE_SH_SIMPLE_PROMPT
#endif

typedef enum {
	REDIRECT_INPUT     = 1,
	REDIRECT_OVERWRITE = 2,
	REDIRECT_APPEND    = 3,
	REDIRECT_HEREIS    = 4,
	REDIRECT_IO        = 5
} redir_type;

/* The descrip member of this structure is only used to make debugging
 * output pretty */
struct {int mode; int default_fd; char *descrip;} redir_table[] = {
	{ 0,                         0, "()" },
	{ O_RDONLY,                  0, "<"  },
	{ O_CREAT|O_TRUNC|O_WRONLY,  1, ">"  },
	{ O_CREAT|O_APPEND|O_WRONLY, 1, ">>" },
	{ O_RDONLY,                 -1, "<<" },
	{ O_RDWR,                    1, "<>" }
};

typedef enum {
	PIPE_SEQ = 1,
	PIPE_AND = 2,
	PIPE_OR  = 3,
	PIPE_BG  = 4,
} pipe_style;

/* might eventually control execution */
typedef enum {
	RES_NONE  = 0,
	RES_IF    = 1,
	RES_THEN  = 2,
	RES_ELIF  = 3,
	RES_ELSE  = 4,
	RES_FI    = 5,
	RES_FOR   = 6,
	RES_WHILE = 7,
	RES_UNTIL = 8,
	RES_DO    = 9,
	RES_DONE  = 10,
	RES_XXXX  = 11,
	RES_SNTX  = 12
} reserved_style;
#define FLAG_END   (1<<RES_NONE)
#define FLAG_IF    (1<<RES_IF)
#define FLAG_THEN  (1<<RES_THEN)
#define FLAG_ELIF  (1<<RES_ELIF)
#define FLAG_ELSE  (1<<RES_ELSE)
#define FLAG_FI    (1<<RES_FI)
#define FLAG_FOR   (1<<RES_FOR)
#define FLAG_WHILE (1<<RES_WHILE)
#define FLAG_UNTIL (1<<RES_UNTIL)
#define FLAG_DO    (1<<RES_DO)
#define FLAG_DONE  (1<<RES_DONE)
#define FLAG_START (1<<RES_XXXX)

/* This holds pointers to the various results of parsing */
struct p_context {
	struct child_prog *child;
	struct pipe *list_head;
	struct pipe *pipe;
	struct redir_struct *pending_redirect;
	reserved_style w;
	int old_flag;				/* for figuring out valid reserved words */
	struct p_context *stack;
	/* How about quoting status? */
};

struct redir_struct {
	redir_type type;			/* type of redirection */
	int fd;						/* file descriptor being redirected */
	int dup;					/* -1, or file descriptor being duplicated */
	struct redir_struct *next;	/* pointer to the next redirect in the list */ 
	glob_t word;				/* *word.gl_pathv is the filename */
};

struct child_prog {
	pid_t pid;					/* 0 if exited */
	char **argv;				/* program name and arguments */
	struct pipe *group;			/* if non-NULL, first in group or subshell */
	int subshell;				/* flag, non-zero if group must be forked */
	struct redir_struct *redirects;	/* I/O redirections */
	glob_t glob_result;			/* result of parameter globbing */
	int is_stopped;				/* is the program currently running? */
	struct pipe *family;		/* pointer back to the child's parent pipe */
};

struct pipe {
	int jobid;					/* job number */
	int num_progs;				/* total number of programs in job */
	int running_progs;			/* number of programs running */
	char *text;					/* name of job */
	char *cmdbuf;				/* buffer various argv's point into */
	pid_t pgrp;					/* process group ID for the job */
	struct child_prog *progs;	/* array of commands in pipe */
	struct pipe *next;			/* to track background commands */
	int stopped_progs;			/* number of programs alive, but stopped */
	int job_context;			/* bitmask defining current context */
	pipe_style followup;		/* PIPE_BG, PIPE_SEQ, PIPE_OR, PIPE_AND */
	reserved_style r_mode;		/* supports if, for, while, until */
	struct jobset *job_list;
};

struct jobset {
	struct pipe *head;			/* head of list of running jobs */
	struct pipe *fg;			/* current foreground job */
};

struct close_me {
	int fd;
	struct close_me *next;
};

/* globals, connect us to the outside world
 * the first three support $?, $#, and $1 */
char **global_argv;
unsigned int global_argc;
unsigned int last_return_code;
extern char **environ; /* This is in <unistd.h>, but protected with __USE_GNU */
 
/* Variables we export */
unsigned int shell_context;  /* Used in cmdedit.c to reset the
                              * context when someone hits ^C */

/* "globals" within this file */
static char *ifs=NULL;
static char map[256];
static int fake_mode=0;
static int interactive=0;
static struct close_me *close_me_head = NULL;
static char *cwd;
/* static struct jobset job_list = { NULL, NULL }; */
static unsigned int last_bg_pid=0;
static char *PS1;
static char *PS2 = "> ";

#define B_CHUNK (100)
#define B_NOSPAC 1
#define MAX_LINE 256       /* for cwd */
#define MAX_READ 256       /* for builtin_read */

typedef struct {
	char *data;
	int length;
	int maxlen;
	int quote;
	int nonnull;
} o_string;
#define NULL_O_STRING {NULL,0,0,0,0}
/* used for initialization:
	o_string foo = NULL_O_STRING; */

/* I can almost use ordinary FILE *.  Is open_memstream() universally
 * available?  Where is it documented? */
struct in_str {
	const char *p;
	int __promptme;
	int promptmode;
	FILE *file;
	int (*get) (struct in_str *);
	int (*peek) (struct in_str *);
};
#define b_getch(input) ((input)->get(input))
#define b_peek(input) ((input)->peek(input))

#define JOB_STATUS_FORMAT "[%d] %-22s %.40s\n"

struct built_in_command {
	char *cmd;					/* name */
	char *descr;				/* description */
	int (*function) (struct child_prog *);	/* function ptr */
};

/* belongs in busybox.h */
static inline int max(int a, int b) {
	return (a>b)?a:b;
}

/* This should be in utility.c */
#ifdef DEBUG_SHELL
static void debug_printf(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}
#else
static void debug_printf(const char *format, ...) { }
#endif
#define final_printf debug_printf

void __syntax(char *file, int line) {
	fprintf(stderr,"syntax error %s:%d\n",file,line);
}
#define syntax() __syntax(__FILE__, __LINE__)

/* Index of subroutines: */
/*   function prototypes for builtins */
static int builtin_cd(struct child_prog *child);
static int builtin_env(struct child_prog *child);
static int builtin_exec(struct child_prog *child);
static int builtin_exit(struct child_prog *child);
static int builtin_export(struct child_prog *child);
static int builtin_fg_bg(struct child_prog *child);
static int builtin_help(struct child_prog *child);
static int builtin_jobs(struct child_prog *child);
static int builtin_pwd(struct child_prog *child);
static int builtin_read(struct child_prog *child);
static int builtin_shift(struct child_prog *child);
static int builtin_source(struct child_prog *child);
static int builtin_ulimit(struct child_prog *child);
static int builtin_umask(struct child_prog *child);
static int builtin_unset(struct child_prog *child);
/*   o_string manipulation: */
static int b_check_space(o_string *o, int len);
static int b_addchr(o_string *o, int ch);
static void b_reset(o_string *o);
static int b_addqchr(o_string *o, int ch, int quote);
static int b_adduint(o_string *o, unsigned int i);
/*  in_str manipulations: */
static int static_get(struct in_str *i);
static int static_peek(struct in_str *i);
static int file_get(struct in_str *i);
static int file_peek(struct in_str *i);
static void setup_file_in_str(struct in_str *i, FILE *f);
static void setup_string_in_str(struct in_str *i, const char *s);
/*  close_me manipulations: */
static void mark_open(int fd);
static void mark_closed(int fd);
static void close_all();
/*  "run" the final data structures: */
static char *indenter(int i);
static int run_list_test(struct pipe *head, int indent);
static int run_pipe_test(struct pipe *pi, int indent);
/*  really run the final data structures: */
static int setup_redirects(struct child_prog *prog, int squirrel[]);
static int pipe_wait(struct pipe *pi);
static int run_list_real(struct pipe *pi);
static void pseudo_exec(struct child_prog *child) __attribute__ ((noreturn));
static int run_pipe_real(struct pipe *pi);
/*   extended glob support: */
static int globhack(const char *src, int flags, glob_t *pglob);
static int glob_needed(const char *s);
static int xglob(o_string *dest, int flags, glob_t *pglob);
/*   data structure manipulation: */
static int setup_redirect(struct p_context *ctx, int fd, redir_type style, struct in_str *input);
static void initialize_context(struct p_context *ctx);
static int done_word(o_string *dest, struct p_context *ctx);
static int done_command(struct p_context *ctx);
static int done_pipe(struct p_context *ctx, pipe_style type);
/*   primary string parsing: */
static int redirect_dup_num(struct in_str *input);
static int redirect_opt_num(o_string *o);
static int process_command_subs(o_string *dest, struct p_context *ctx, struct in_str *input, int subst_end);
static int parse_group(o_string *dest, struct p_context *ctx, struct in_str *input, int ch);
static void lookup_param(o_string *dest, struct p_context *ctx, o_string *src);
static int handle_dollar(o_string *dest, struct p_context *ctx, struct in_str *input);
static int parse_string(o_string *dest, struct p_context *ctx, const char *src);
static int parse_stream(o_string *dest, struct p_context *ctx, struct in_str *input0, int end_trigger);
/*   setup: */
static int parse_stream_outer(struct in_str *inp);
static int parse_string_outer(const char *s);
static int parse_file_outer(FILE *f);

/* Table of built-in functions.  They can be forked or not, depending on
 * context: within pipes, they fork.  As simple commands, they do not.
 * When used in non-forking context, they can change global variables
 * in the parent shell process.  If forked, of course they can not.
 * For example, 'unset foo | whatever' will parse and run, but foo will
 * still be set at the end. */
static struct built_in_command bltins[] = {
	{"bg", "Resume a job in the background", builtin_fg_bg},
	{"cd", "Change working directory", builtin_cd},
	{"env", "Print all environment variables", builtin_env},
	{"exec", "Exec command, replacing this shell with the exec'd process", builtin_exec},
	{"exit", "Exit from shell()", builtin_exit},
	{"export", "Set environment variable", builtin_export},
	{"fg", "Bring job into the foreground", builtin_fg_bg},
	{"jobs", "Lists the active jobs", builtin_jobs},
	{"pwd", "Print current directory", builtin_pwd},
	{"read", "Input environment variable", builtin_read},
	{"shift", "Shift positional parameters", builtin_shift},
	{"ulimit","Controls resource limits", builtin_ulimit},
	{"umask","Sets file creation mask", builtin_umask},
	{"unset", "Unset environment variable", builtin_unset},
	{".", "Source-in and run commands in a file", builtin_source},
	{"help", "List shell built-in commands", builtin_help},
	{NULL, NULL, NULL}
};

/* built-in 'cd <path>' handler */
static int builtin_cd(struct child_prog *child)
{
	char *newdir;
	if (child->argv[1] == NULL)
		newdir = getenv("HOME");
	else
		newdir = child->argv[1];
	if (chdir(newdir)) {
		printf("cd: %s: %s\n", newdir, strerror(errno));
		return EXIT_FAILURE;
	}
	getcwd(cwd, sizeof(char)*MAX_LINE);
	return EXIT_SUCCESS;
}

/* built-in 'env' handler */
static int builtin_env(struct child_prog *dummy)
{
	char **e = environ;
	if (e == NULL) return EXIT_FAILURE;
	for (; *e; e++) {
		puts(*e);
	}
	return EXIT_SUCCESS;
}

/* built-in 'exec' handler */
static int builtin_exec(struct child_prog *child)
{
	if (child->argv[1] == NULL)
		return EXIT_SUCCESS;   /* Really? */
	child->argv++;
	pseudo_exec(child);
	/* never returns */
}

/* built-in 'exit' handler */
static int builtin_exit(struct child_prog *child)
{
	if (child->argv[1] == NULL)
		exit(EXIT_SUCCESS);
	exit (atoi(child->argv[1]));
}

/* built-in 'export VAR=value' handler */
static int builtin_export(struct child_prog *child)
{
	int res;

	if (child->argv[1] == NULL) {
		return (builtin_env(child));
	}
	res = putenv(child->argv[1]);
	if (res)
		fprintf(stderr, "export: %s\n", strerror(errno));
	return (res);
}

/* built-in 'fg' and 'bg' handler */
static int builtin_fg_bg(struct child_prog *child)
{
	int i, jobNum;
	struct pipe *job=NULL;
	
	if (!child->argv[1] || child->argv[2]) {
		error_msg("%s: exactly one argument is expected\n",
				child->argv[0]);
		return EXIT_FAILURE;
	}

	if (sscanf(child->argv[1], "%%%d", &jobNum) != 1) {
		error_msg("%s: bad argument '%s'\n",
				child->argv[0], child->argv[1]);
		return EXIT_FAILURE;
	}

	for (job = child->family->job_list->head; job; job = job->next) {
		if (job->jobid == jobNum) {
			break;
		}
	}

	if (!job) {
		error_msg("%s: unknown job %d\n",
				child->argv[0], jobNum);
		return EXIT_FAILURE;
	}

	if (*child->argv[0] == 'f') {
		/* Make this job the foreground job */
		/* suppress messages when run from /linuxrc mag@sysgo.de */
		if (tcsetpgrp(0, job->pgrp) && errno != ENOTTY)
			perror_msg("tcsetpgrp"); 
		child->family->job_list->fg = job;
	}

	/* Restart the processes in the job */
	for (i = 0; i < job->num_progs; i++)
		job->progs[i].is_stopped = 0;

	kill(-job->pgrp, SIGCONT);

	job->stopped_progs = 0;
	return EXIT_SUCCESS;
}

/* built-in 'help' handler */
static int builtin_help(struct child_prog *dummy)
{
	struct built_in_command *x;

	printf("\nBuilt-in commands:\n");
	printf("-------------------\n");
	for (x = bltins; x->cmd; x++) {
		if (x->descr==NULL)
			continue;
		printf("%s\t%s\n", x->cmd, x->descr);
	}
	printf("\n\n");
	return EXIT_SUCCESS;
}

/* built-in 'jobs' handler */
static int builtin_jobs(struct child_prog *child)
{
	struct pipe *job;
	char *status_string;

	for (job = child->family->job_list->head; job; job = job->next) {
		if (job->running_progs == job->stopped_progs)
			status_string = "Stopped";
		else
			status_string = "Running";
		printf(JOB_STATUS_FORMAT, job->jobid, status_string, job->text);
	}
	return EXIT_SUCCESS;
}


/* built-in 'pwd' handler */
static int builtin_pwd(struct child_prog *dummy)
{
	getcwd(cwd, MAX_LINE);
	puts(cwd);
	return EXIT_SUCCESS;
}

/* built-in 'read VAR' handler */
static int builtin_read(struct child_prog *child)
{
	int res = 0, len, newlen;
	char *s;
	char string[MAX_READ];

	if (child->argv[1]) {
		/* argument (VAR) given: put "VAR=" into buffer */
		strcpy(string, child->argv[1]);
		len = strlen(string);
		string[len++] = '=';
		string[len]   = '\0';
		/* XXX would it be better to go through in_str? */
		fgets(&string[len], sizeof(string) - len, stdin);	/* read string */
		newlen = strlen(string);
		if(newlen > len)
			string[--newlen] = '\0';	/* chomp trailing newline */
		/*
		** string should now contain "VAR=<value>"
		** copy it (putenv() won't do that, so we must make sure
		** the string resides in a static buffer!)
		*/
		res = -1;
		if((s = strdup(string)))
			res = putenv(s);
		if (res)
			fprintf(stderr, "read: %s\n", strerror(errno));
	}
	else
		fgets(string, sizeof(string), stdin);

	return (res);
}

/* Built-in 'shift' handler */
static int builtin_shift(struct child_prog *child)
{
	int n=1;
	if (child->argv[1]) {
		n=atoi(child->argv[1]);
	}
	if (n>=0 && n<global_argc) {
		/* XXX This probably breaks $0 */
		global_argc -= n;
		global_argv += n;
		return EXIT_SUCCESS;
	} else {
		return EXIT_FAILURE;
	}
}

/* Built-in '.' handler (read-in and execute commands from file) */
static int builtin_source(struct child_prog *child)
{
	FILE *input;
	int status;

	if (child->argv[1] == NULL)
		return EXIT_FAILURE;

	/* XXX search through $PATH is missing */
	input = fopen(child->argv[1], "r");
	if (!input) {
		fprintf(stderr, "Couldn't open file '%s'\n", child->argv[1]);
		return EXIT_FAILURE;
	}

	/* Now run the file */
	/* XXX argv and argc are broken; need to save old global_argv
	 * (pointer only is OK!) on this stack frame,
	 * set global_argv=child->argv+1, recurse, and restore. */
	mark_open(fileno(input));
	status = parse_file_outer(input);
	mark_closed(fileno(input));
	fclose(input);
	return (status);
}

static int builtin_ulimit(struct child_prog *child)
{
	printf("builtin_ulimit not written\n");
	return EXIT_FAILURE;
}

static int builtin_umask(struct child_prog *child)
{
	printf("builtin_umask not written\n");
	return EXIT_FAILURE;
}

/* built-in 'unset VAR' handler */
static int builtin_unset(struct child_prog *child)
{
	if (child->argv[1] == NULL) {
		fprintf(stderr, "unset: parameter required.\n");
		return EXIT_FAILURE;
	}
	unsetenv(child->argv[1]);
	return EXIT_SUCCESS;
}

static int b_check_space(o_string *o, int len)
{
	/* It would be easy to drop a more restrictive policy
	 * in here, such as setting a maximum string length */
	if (o->length + len > o->maxlen) {
		char *old_data = o->data;
		/* assert (data == NULL || o->maxlen != 0); */
		o->maxlen += max(2*len, B_CHUNK);
		o->data = realloc(o->data, 1 + o->maxlen);
		if (o->data == NULL) {
			free(old_data);
		}
	}
	return o->data == NULL;
}

static int b_addchr(o_string *o, int ch)
{
	debug_printf("b_addchr: %c %d %p\n", ch, o->length, o);
	if (b_check_space(o, 1)) return B_NOSPAC;
	o->data[o->length] = ch;
	o->length++;
	o->data[o->length] = '\0';
	return 0;
}

static void b_reset(o_string *o)
{
	o->length = 0;
	o->nonnull = 0;
	if (o->data != NULL) *o->data = '\0';
}

static void b_free(o_string *o)
{
	b_reset(o);
	if (o->data != NULL) free(o->data);
	o->data = NULL;
	o->maxlen = 0;
}

/* My analysis of quoting semantics tells me that state information
 * is associated with a destination, not a source.
 */
static int b_addqchr(o_string *o, int ch, int quote)
{
	if (quote && strchr("*?[\\",ch)) {
		int rc;
		rc = b_addchr(o, '\\');
		if (rc) return rc;
	}
	return b_addchr(o, ch);
}

/* belongs in utility.c */
char *simple_itoa(unsigned int i)
{
	/* 21 digits plus null terminator, good for 64-bit or smaller ints */
	static char local[22];
	char *p = &local[21];
	*p-- = '\0';
	do {
		*p-- = '0' + i % 10;
		i /= 10;
	} while (i > 0);
	return p + 1;
}

static int b_adduint(o_string *o, unsigned int i)
{
	int r;
	char *p = simple_itoa(i);
	/* no escape checking necessary */
	do r=b_addchr(o, *p++); while (r==0 && *p);
	return r;
}

static int static_get(struct in_str *i)
{
	int ch=*i->p++;
	if (ch=='\0') return EOF;
	return ch;
}

static int static_peek(struct in_str *i)
{
	return *i->p;
}

static inline void cmdedit_set_initial_prompt(void)
{
#ifdef BB_FEATURE_SH_SIMPLE_PROMPT
	PS1 = NULL;
#else
	PS1 = getenv("PS1");
	if(PS1==0)
		PS1 = "\\w \\$ ";
#endif	
}

static inline void setup_prompt_string(int promptmode, char **prompt_str)
{
	debug_printf("setup_prompt_string %d ",promptmode);
#ifdef BB_FEATURE_SH_SIMPLE_PROMPT
	/* Set up the prompt */
	if (promptmode == 1) {
		if (PS1)
			free(PS1);
		PS1=xmalloc(strlen(cwd)+4);
		sprintf(PS1, "%s %s", cwd, ( geteuid() != 0 ) ?  "$ ":"# ");
		*prompt_str = PS1;
	} else {
		*prompt_str = PS2;
	}
#else
	*prompt_str = (promptmode==0)? PS1 : PS2;
#endif
	debug_printf("result %s\n",*prompt_str);
}

static void get_user_input(struct in_str *i)
{
	char *prompt_str;
	static char the_command[BUFSIZ];

	setup_prompt_string(i->promptmode, &prompt_str);
#ifdef BB_FEATURE_COMMAND_EDITING
	/*
	 ** enable command line editing only while a command line
	 ** is actually being read; otherwise, we'll end up bequeathing
	 ** atexit() handlers and other unwanted stuff to our
	 ** child processes (rob@sysgo.de)
	 */
	cmdedit_read_input(prompt_str, the_command);
	cmdedit_terminate();
#else
	fputs(prompt_str, stdout);
	fflush(stdout);
	the_command[0]=fgetc(i->file);
	the_command[1]='\0';
#endif
	i->p = the_command;
}

/* This is the magic location that prints prompts 
 * and gets data back from the user */
static int file_get(struct in_str *i)
{
	int ch;

	ch = 0;
	/* If there is data waiting, eat it up */
	if (i->p && *i->p) {
		ch=*i->p++;
	} else {
		/* need to double check i->file because we might be doing something
		 * more complicated by now, like sourcing or substituting. */
		if (i->__promptme && interactive && i->file == stdin) {
			get_user_input(i);
			i->promptmode=2;
		}
		i->__promptme = 0;

		if (i->p && *i->p) {
			ch=*i->p++;
		}
		debug_printf("b_getch: got a %d\n", ch);
	}
	if (ch == '\n') i->__promptme=1;
	return ch;
}

/* All the callers guarantee this routine will never be
 * used right after a newline, so prompting is not needed.
 */
static int file_peek(struct in_str *i)
{
	if (i->p && *i->p) {
		return *i->p;
	} else {
		static char buffer;
		buffer = fgetc(i->file);
		i->p = &buffer;
		debug_printf("b_peek: got a %d\n", *i->p);
		return *i->p; 
	}
}

static void setup_file_in_str(struct in_str *i, FILE *f)
{
	i->peek = file_peek;
	i->get = file_get;
	i->__promptme=1;
	i->promptmode=1;
	i->file = f;
	i->p = NULL;
}

static void setup_string_in_str(struct in_str *i, const char *s)
{
	i->peek = static_peek;
	i->get = static_get;
	i->__promptme=1;
	i->promptmode=1;
	i->p = s;
}

static void mark_open(int fd)
{
	struct close_me *new = xmalloc(sizeof(struct close_me));
	new->fd = fd;
	new->next = close_me_head;
	close_me_head = new;
}

static void mark_closed(int fd)
{
	struct close_me *tmp;
	if (close_me_head == NULL || close_me_head->fd != fd)
		error_msg_and_die("corrupt close_me");
	tmp = close_me_head;
	close_me_head = close_me_head->next;
	free(tmp);
}

static void close_all()
{
	struct close_me *c;
	for (c=close_me_head; c; c=c->next) {
		close(c->fd);
	}
	close_me_head = NULL;
}

/* squirrel != NULL means we squirrel away copies of stdin, stdout,
 * and stderr if they are redirected. */
static int setup_redirects(struct child_prog *prog, int squirrel[])
{
	int openfd, mode;
	struct redir_struct *redir;

	for (redir=prog->redirects; redir; redir=redir->next) {
		if (redir->dup == -1) {
			mode=redir_table[redir->type].mode;
			openfd = open(redir->word.gl_pathv[0], mode, 0666);
			if (openfd < 0) {
			/* this could get lost if stderr has been redirected, but
			   bash and ash both lose it as well (though zsh doesn't!) */
				fprintf(stderr,"error opening %s: %s\n", redir->word.gl_pathv[0],
					strerror(errno));
				return 1;
			}
		} else {
			openfd = redir->dup;
		}

		if (openfd != redir->fd) {
			if (squirrel && redir->fd < 3) {
				squirrel[redir->fd] = dup(redir->fd);
			}
			dup2(openfd, redir->fd);
			close(openfd);
		}
	}
	return 0;
}

static void restore_redirects(int squirrel[])
{
	int i, fd;
	for (i=0; i<3; i++) {
		fd = squirrel[i];
		if (fd != -1) {
			/* No error checking.  I sure wouldn't know what
			 * to do with an error if I found one! */
			dup2(fd, i);
			close(fd);
		}
	}
}

/* XXX this definitely needs some more thought, work, and
 * cribbing from other shells */
static int pipe_wait(struct pipe *pi)
{
	int rcode=0, i, pid, running, status;
	running = pi->num_progs;
	while (running) {
		pid=waitpid(-1, &status, 0);
		if (pid < 0) perror_msg_and_die("waitpid");
		for (i=0; i < pi->num_progs; i++) {
			if (pi->progs[i].pid == pid) {
				if (i==pi->num_progs-1) rcode=WEXITSTATUS(status);
				pi->progs[i].pid = 0;
				running--;
				break;
			}
		}
	}
	return rcode;
}

/* very simple version for testing */
static void pseudo_exec(struct child_prog *child)
{
	int rcode;
	struct built_in_command *x;
	if (child->argv) {
		/*
		 * Check if the command matches any of the builtins.
		 * Depending on context, this might be redundant.  But it's
		 * easier to waste a few CPU cycles than it is to figure out
		 * if this is one of those cases.
		 */
		for (x = bltins; x->cmd; x++) {
			if (strcmp(child->argv[0], x->cmd) == 0 ) {
				debug_printf("builtin exec %s\n", child->argv[0]);
				exit(x->function(child));
			}
		}

		/* Check if the command matches any busybox internal commands
		 * ("applets") here.  
		 * FIXME: This feature is not 100% safe, since
		 * BusyBox is not fully reentrant, so we have no guarantee the things
		 * from the .bss are still zeroed, or that things from .data are still
		 * at their defaults.  We could exec ourself from /proc/self/exe, but I
		 * really dislike relying on /proc for things.  We could exec ourself
		 * from global_argv[0], but if we are in a chroot, we may not be able
		 * to find ourself... */ 
#ifdef BB_FEATURE_SH_STANDALONE_SHELL
		{
			int argc_l;
			char** argv_l=child->argv;
			char *name = child->argv[0];

#ifdef BB_FEATURE_SH_APPLETS_ALWAYS_WIN
			/* Following discussions from November 2000 on the busybox mailing
			 * list, the default configuration, (without
			 * get_last_path_component()) lets the user force use of an
			 * external command by specifying the full (with slashes) filename.
			 * If you enable BB_FEATURE_SH_APPLETS_ALWAYS_WIN, then applets
			 * _aways_ override external commands, so if you want to run
			 * /bin/cat, it will use BusyBox cat even if /bin/cat exists on the
			 * filesystem and is _not_ busybox.  Some systems may want this,
			 * most do not.  */
			name = get_last_path_component(name);
#endif
			/* Count argc for use in a second... */
			for(argc_l=0;*argv_l!=NULL; argv_l++, argc_l++);
			optind = 1;
			debug_printf("running applet %s\n", name);
			run_applet_by_name(name, argc_l, child->argv);
			exit(1);
		}
#endif
		debug_printf("exec of %s\n",child->argv[0]);
		execvp(child->argv[0],child->argv);
		perror("execvp");
		exit(1);
	} else if (child->group) {
		debug_printf("runtime nesting to group\n");
		interactive=0;    /* crucial!!!! */
		rcode = run_list_real(child->group);
		/* OK to leak memory by not calling run_list_test,
		 * since this process is about to exit */
		exit(rcode);
	} else {
		/* Can happen.  See what bash does with ">foo" by itself. */
		debug_printf("trying to pseudo_exec null command\n");
		exit(EXIT_SUCCESS);
	}
}

/* run_pipe_real() starts all the jobs, but doesn't wait for anything
 * to finish.  See pipe_wait().
 *
 * return code is normally -1, when the caller has to wait for children
 * to finish to determine the exit status of the pipe.  If the pipe
 * is a simple builtin command, however, the action is done by the
 * time run_pipe_real returns, and the exit code is provided as the
 * return value.
 *
 * The input of the pipe is always stdin, the output is always
 * stdout.  The outpipe[] mechanism in BusyBox-0.48 lash is bogus,
 * because it tries to avoid running the command substitution in
 * subshell, when that is in fact necessary.  The subshell process
 * now has its stdout directed to the input of the appropriate pipe,
 * so this routine is noticeably simpler.
 */
static int run_pipe_real(struct pipe *pi)
{
	int i;
	int nextin, nextout;
	int pipefds[2];				/* pipefds[0] is for reading */
	struct child_prog *child;
	struct built_in_command *x;

	nextin = 0;
	pi->pgrp = 0;

	/* Check if this is a simple builtin (not part of a pipe).
	 * Builtins within pipes have to fork anyway, and are handled in
	 * pseudo_exec.  "echo foo | read bar" doesn't work on bash, either.
	 */
	if (pi->num_progs == 1 && pi->progs[0].argv != NULL) {
		child = & (pi->progs[0]);
		if (child->group && ! child->subshell) {
			int squirrel[] = {-1, -1, -1};
			int rcode;
			debug_printf("non-subshell grouping\n");
			setup_redirects(child, squirrel);
			/* XXX could we merge code with following builtin case,
			 * by creating a pseudo builtin that calls run_list_real? */
			rcode = run_list_real(child->group);
			restore_redirects(squirrel);
			return rcode;
		}
		for (x = bltins; x->cmd; x++) {
			if (strcmp(child->argv[0], x->cmd) == 0 ) {
				int squirrel[] = {-1, -1, -1};
				int rcode;
				debug_printf("builtin inline %s\n", child->argv[0]);
				/* XXX setup_redirects acts on file descriptors, not FILEs.
				 * This is perfect for work that comes after exec().
				 * Is it really safe for inline use?  Experimentally,
				 * things seem to work with glibc. */
				setup_redirects(child, squirrel);
				rcode = x->function(child);
				restore_redirects(squirrel);
				return rcode;
			}
		}
	}

	for (i = 0; i < pi->num_progs; i++) {
		child = & (pi->progs[i]);

		/* pipes are inserted between pairs of commands */
		if ((i + 1) < pi->num_progs) {
			if (pipe(pipefds)<0) perror_msg_and_die("pipe");
			nextout = pipefds[1];
		} else {
			nextout=1;
			pipefds[0] = -1;
		}

		/* XXX test for failed fork()? */
		if (!(child->pid = fork())) {
			close_all();

			if (nextin != 0) {
				dup2(nextin, 0);
				close(nextin);
			}
			if (nextout != 1) {
				dup2(nextout, 1);
				close(nextout);
			}
			if (pipefds[0]!=-1) {
				close(pipefds[0]);  /* opposite end of our output pipe */
			}

			/* Like bash, explicit redirects override pipes,
			 * and the pipe fd is available for dup'ing. */
			setup_redirects(child,NULL);

			pseudo_exec(child);
		}
		if (interactive) {
			/* Put our child in the process group whose leader is the
			 * first process in this pipe. */
			if (pi->pgrp==0) {
				pi->pgrp = child->pid;
			}
			/* Don't check for errors.  The child may be dead already,
			 * in which case setpgid returns error code EACCES. */
			setpgid(child->pid, pi->pgrp);
		}
		/* In the non-interactive case, do nothing.  Leave the children
		 * with the process group that they inherited from us. */
	
		if (nextin != 0)
			close(nextin);
		if (nextout != 1)
			close(nextout);

		/* If there isn't another process, nextin is garbage 
		   but it doesn't matter */
		nextin = pipefds[0];
	}
	return -1;
}

static int run_list_real(struct pipe *pi)
{
	int rcode=0;
	int if_code=0, next_if_code=0;  /* need double-buffer to handle elif */
	reserved_style rmode=RES_NONE;
	for (;pi;pi=pi->next) {
		rmode = pi->r_mode;
		debug_printf("rmode=%d  if_code=%d  next_if_code=%d\n", rmode, if_code, next_if_code);
		if (rmode == RES_THEN || rmode == RES_ELSE) if_code = next_if_code;
		if (rmode == RES_THEN &&  if_code) continue;
		if (rmode == RES_ELSE && !if_code) continue;
		if (rmode == RES_ELIF && !if_code) continue;
		if (pi->num_progs == 0) break;
		rcode = run_pipe_real(pi);
		if (rcode!=-1) {
			/* We only ran a builtin: rcode was set by the return value
			 * of run_pipe_real(), and we don't need to wait for anything. */
		} else if (pi->followup==PIPE_BG) {
			/* XXX check bash's behavior with nontrivial pipes */
			/* XXX compute jobid */
			/* XXX what does bash do with attempts to background builtins? */
			printf("[%d] %d\n", pi->jobid, pi->pgrp);
			last_bg_pid = pi->pgrp;
			rcode = EXIT_SUCCESS;
		} else {
			if (interactive) {
				/* move the new process group into the foreground */
				/* suppress messages when run from /linuxrc mag@sysgo.de */
				signal(SIGTTIN, SIG_IGN);
				signal(SIGTTOU, SIG_IGN);
				if (tcsetpgrp(0, pi->pgrp) && errno != ENOTTY)
					perror_msg("tcsetpgrp");
				rcode = pipe_wait(pi);
				if (tcsetpgrp(0, getpid()) && errno != ENOTTY)
					perror_msg("tcsetpgrp");
				signal(SIGTTIN, SIG_DFL);
				signal(SIGTTOU, SIG_DFL);
			} else {
				rcode = pipe_wait(pi);
			}
		}
		last_return_code=rcode;
		if ( rmode == RES_IF || rmode == RES_ELIF )
			next_if_code=rcode;  /* can be overwritten a number of times */
		if ( (rcode==EXIT_SUCCESS && pi->followup==PIPE_OR) ||
		     (rcode!=EXIT_SUCCESS && pi->followup==PIPE_AND) )
			return rcode;  /* XXX broken if list is part of if/then/else */
	}
	return rcode;
}

/* broken, of course, but OK for testing */
static char *indenter(int i)
{
	static char blanks[]="                                    ";
	return &blanks[sizeof(blanks)-i-1];
}

/* return code is the exit status of the pipe */
static int run_pipe_test(struct pipe *pi, int indent)
{
	char **p;
	struct child_prog *child;
	struct redir_struct *r, *rnext;
	int a, i, ret_code=0;
	char *ind = indenter(indent);
	final_printf("%s run pipe: (pid %d)\n",ind,getpid());
	for (i=0; i<pi->num_progs; i++) {
		child = &pi->progs[i];
		final_printf("%s  command %d:\n",ind,i);
		if (child->argv) {
			for (a=0,p=child->argv; *p; a++,p++) {
				final_printf("%s   argv[%d] = %s\n",ind,a,*p);
			}
			globfree(&child->glob_result);
			child->argv=NULL;
		} else if (child->group) {
			final_printf("%s   begin group (subshell:%d)\n",ind, child->subshell);
			ret_code = run_list_test(child->group,indent+3);
			final_printf("%s   end group\n",ind);
		} else {
			final_printf("%s   (nil)\n",ind);
		}
		for (r=child->redirects; r; r=rnext) {
			final_printf("%s   redirect %d%s", ind, r->fd, redir_table[r->type].descrip);
			if (r->dup == -1) {
				final_printf(" %s\n", *r->word.gl_pathv);
				globfree(&r->word);
			} else {
				final_printf("&%d\n", r->dup);
			}
			rnext=r->next;
			free(r);
		}
		child->redirects=NULL;
	}
	free(pi->progs);   /* children are an array, they get freed all at once */
	pi->progs=NULL;
	return ret_code;
}

static int run_list_test(struct pipe *head, int indent)
{
	int rcode=0;   /* if list has no members */
	struct pipe *pi, *next;
	char *ind = indenter(indent);
	for (pi=head; pi; pi=next) {
		if (pi->num_progs == 0) break;
		final_printf("%s pipe reserved mode %d\n", ind, pi->r_mode);
		rcode = run_pipe_test(pi, indent);
		final_printf("%s pipe followup code %d\n", ind, pi->followup);
		next=pi->next;
		pi->next=NULL;
		free(pi);
	}
	return rcode;	
}

/* Select which version we will use */
static int run_list(struct pipe *pi)
{
	int rcode=0;
	if (fake_mode==0) {
		rcode = run_list_real(pi);
	} 
	/* run_list_test has the side effect of clearing memory
	 * In the long run that function can be merged with run_list_real,
	 * but doing that now would hobble the debugging effort. */
	run_list_test(pi,0);
	return rcode;
}

/* The API for glob is arguably broken.  This routine pushes a non-matching
 * string into the output structure, removing non-backslashed backslashes.
 * If someone can prove me wrong, by performing this function within the
 * original glob(3) api, feel free to rewrite this routine into oblivion.
 * Return code (0 vs. GLOB_NOSPACE) matches glob(3).
 * XXX broken if the last character is '\\', check that before calling.
 */
static int globhack(const char *src, int flags, glob_t *pglob)
{
	int cnt, pathc;
	const char *s;
	char *dest;
	for (cnt=1, s=src; *s; s++) {
		if (*s == '\\') s++;
		cnt++;
	}
	dest = malloc(cnt);
	if (!dest) return GLOB_NOSPACE;
	if (!(flags & GLOB_APPEND)) {
		pglob->gl_pathv=NULL;
		pglob->gl_pathc=0;
		pglob->gl_offs=0;
		pglob->gl_offs=0;
	}
	pathc = ++pglob->gl_pathc;
	pglob->gl_pathv = realloc(pglob->gl_pathv, (pathc+1)*sizeof(*pglob->gl_pathv));
	if (pglob->gl_pathv == NULL) return GLOB_NOSPACE;
	pglob->gl_pathv[pathc-1]=dest;
	pglob->gl_pathv[pathc]=NULL;
	for (s=src; *s; s++, dest++) {
		if (*s == '\\') s++;
		*dest = *s;
	}
	*dest='\0';
	return 0;
}

/* XXX broken if the last character is '\\', check that before calling */
static int glob_needed(const char *s)
{
	for (; *s; s++) {
		if (*s == '\\') s++;
		if (strchr("*[?",*s)) return 1;
	}
	return 0;
}

#if 0
static void globprint(glob_t *pglob)
{
	int i;
	debug_printf("glob_t at %p:\n", pglob);
	debug_printf("  gl_pathc=%d  gl_pathv=%p  gl_offs=%d  gl_flags=%d\n",
		pglob->gl_pathc, pglob->gl_pathv, pglob->gl_offs, pglob->gl_flags);
	for (i=0; i<pglob->gl_pathc; i++)
		debug_printf("pglob->gl_pathv[%d] = %p = %s\n", i,
			pglob->gl_pathv[i], pglob->gl_pathv[i]);
}
#endif

static int xglob(o_string *dest, int flags, glob_t *pglob)
{
	int gr;

 	/* short-circuit for null word */
	/* we can code this better when the debug_printf's are gone */
 	if (dest->length == 0) {
 		if (dest->nonnull) {
 			/* bash man page calls this an "explicit" null */
 			gr = globhack(dest->data, flags, pglob);
 			debug_printf("globhack returned %d\n",gr);
 		} else {
			return 0;
		}
 	} else if (glob_needed(dest->data)) {
		gr = glob(dest->data, flags, NULL, pglob);
		debug_printf("glob returned %d\n",gr);
		if (gr == GLOB_NOMATCH) {
			/* quote removal, or more accurately, backslash removal */
			gr = globhack(dest->data, flags, pglob);
			debug_printf("globhack returned %d\n",gr);
		}
	} else {
		gr = globhack(dest->data, flags, pglob);
		debug_printf("globhack returned %d\n",gr);
	}
	if (gr == GLOB_NOSPACE) {
		fprintf(stderr,"out of memory during glob\n");
		exit(1);
	}
	if (gr != 0) { /* GLOB_ABORTED ? */
		fprintf(stderr,"glob(3) error %d\n",gr);
	}
	/* globprint(glob_target); */
	return gr;
}

/* the src parameter allows us to peek forward to a possible &n syntax
 * for file descriptor duplication, e.g., "2>&1".
 * Return code is 0 normally, 1 if a syntax error is detected in src.
 * Resource errors (in xmalloc) cause the process to exit */
static int setup_redirect(struct p_context *ctx, int fd, redir_type style,
	struct in_str *input)
{
	struct child_prog *child=ctx->child;
	struct redir_struct *redir = child->redirects;
	struct redir_struct *last_redir=NULL;

	/* Create a new redir_struct and drop it onto the end of the linked list */
	while(redir) {
		last_redir=redir;
		redir=redir->next;
	}
	redir = xmalloc(sizeof(struct redir_struct));
	redir->next=NULL;
	if (last_redir) {
		last_redir->next=redir;
	} else {
		child->redirects=redir;
	}

	redir->type=style;
	redir->fd= (fd==-1) ? redir_table[style].default_fd : fd ;

	debug_printf("Redirect type %d%s\n", redir->fd, redir_table[style].descrip);

	/* Check for a '2>&1' type redirect */ 
	redir->dup = redirect_dup_num(input);
	if (redir->dup == -2) return 1;  /* syntax error */
	if (redir->dup != -1) {
		/* Erik had a check here that the file descriptor in question
		 * is legit; I postpone that to "run time" */
		debug_printf("Duplicating redirect '%d>&%d'\n", redir->fd, redir->dup);
	} else {
		/* We do _not_ try to open the file that src points to,
		 * since we need to return and let src be expanded first.
		 * Set ctx->pending_redirect, so we know what to do at the
		 * end of the next parsed word.
		 */
		ctx->pending_redirect = redir;
	}
	return 0;
}

struct pipe *new_pipe(void) {
	struct pipe *pi;
	pi = xmalloc(sizeof(struct pipe));
	pi->num_progs = 0;
	pi->progs = NULL;
	pi->next = NULL;
	pi->followup = 0;  /* invalid */
	return pi;
}

static void initialize_context(struct p_context *ctx)
{
	ctx->pipe=NULL;
	ctx->pending_redirect=NULL;
	ctx->child=NULL;
	ctx->list_head=new_pipe();
	ctx->pipe=ctx->list_head;
	ctx->w=RES_NONE;
	ctx->stack=NULL;
	done_command(ctx);   /* creates the memory for working child */
}

/* normal return is 0
 * if a reserved word is found, and processed, return 1
 * should handle if, then, elif, else, fi, for, while, until, do, done.
 * case, function, and select are obnoxious, save those for later.
 */
int reserved_word(o_string *dest, struct p_context *ctx)
{
	struct reserved_combo {
		char *literal;
		int code;
		long flag;
	};
	/* Mostly a list of accepted follow-up reserved words.
	 * FLAG_END means we are done with the sequence, and are ready
	 * to turn the compound list into a command.
	 * FLAG_START means the word must start a new compound list.
	 */
	static struct reserved_combo reserved_list[] = {
		{ "if",    RES_IF,    FLAG_THEN | FLAG_START },
		{ "then",  RES_THEN,  FLAG_ELIF | FLAG_ELSE | FLAG_FI },
		{ "elif",  RES_ELIF,  FLAG_THEN },
		{ "else",  RES_ELSE,  FLAG_FI   },
		{ "fi",    RES_FI,    FLAG_END  },
		{ "for",   RES_FOR,   FLAG_DO   | FLAG_START },
		{ "while", RES_WHILE, FLAG_DO   | FLAG_START },
		{ "until", RES_UNTIL, FLAG_DO   | FLAG_START },
		{ "do",    RES_DO,    FLAG_DONE },
		{ "done",  RES_DONE,  FLAG_END  }
	};
	struct reserved_combo *r;
	for (r=reserved_list;
#define NRES sizeof(reserved_list)/sizeof(struct reserved_combo)
		r<reserved_list+NRES; r++) {
		if (strcmp(dest->data, r->literal) == 0) {
			debug_printf("found reserved word %s, code %d\n",r->literal,r->code);
			if (r->flag & FLAG_START) {
				struct p_context *new = xmalloc(sizeof(struct p_context));
				debug_printf("push stack\n");
				*new = *ctx;   /* physical copy */
				initialize_context(ctx);
				ctx->stack=new;
			} else if ( ctx->w == RES_NONE || ! (ctx->old_flag & (1<<r->code))) {
				syntax();
				ctx->w = RES_SNTX;
				b_reset (dest);
				return 1;
			}
			ctx->w=r->code;
			ctx->old_flag = r->flag;
			if (ctx->old_flag & FLAG_END) {
				struct p_context *old;
				debug_printf("pop stack\n");
				old = ctx->stack;
				old->child->group = ctx->list_head;
				*ctx = *old;   /* physical copy */
				free(old);
				ctx->w=RES_NONE;
			}
			b_reset (dest);
			return 1;
		}
	}
	return 0;
}

/* normal return is 0.
 * Syntax or xglob errors return 1. */
static int done_word(o_string *dest, struct p_context *ctx)
{
	struct child_prog *child=ctx->child;
	glob_t *glob_target;
	int gr, flags = 0;

	debug_printf("done_word: %s %p\n", dest->data, child);
	if (dest->length == 0 && !dest->nonnull) {
		debug_printf("  true null, ignored\n");
		return 0;
	}
	if (ctx->pending_redirect) {
		glob_target = &ctx->pending_redirect->word;
	} else {
		if (child->group) {
			syntax();
			return 1;  /* syntax error, groups and arglists don't mix */
		}
		if (!child->argv) {
			debug_printf("checking %s for reserved-ness\n",dest->data);
			if (reserved_word(dest,ctx)) return ctx->w==RES_SNTX;
		}
		glob_target = &child->glob_result;
 		if (child->argv) flags |= GLOB_APPEND;
	}
	gr = xglob(dest, flags, glob_target);
	if (gr != 0) return 1;

	b_reset(dest);
	if (ctx->pending_redirect) {
		ctx->pending_redirect=NULL;
		if (glob_target->gl_pathc != 1) {
			fprintf(stderr, "ambiguous redirect\n");
			return 1;
		}
	} else {
		child->argv = glob_target->gl_pathv;
	}
	return 0;
}

/* The only possible error here is out of memory, in which case
 * xmalloc exits. */
static int done_command(struct p_context *ctx)
{
	/* The child is really already in the pipe structure, so
	 * advance the pipe counter and make a new, null child.
	 * Only real trickiness here is that the uncommitted
	 * child structure, to which ctx->child points, is not
	 * counted in pi->num_progs. */
	struct pipe *pi=ctx->pipe;
	struct child_prog *prog=ctx->child;

	if (prog && prog->group == NULL
	         && prog->argv == NULL
	         && prog->redirects == NULL) {
		debug_printf("done_command: skipping null command\n");
		return 0;
	} else if (prog) {
		pi->num_progs++;
		debug_printf("done_command: num_progs incremented to %d\n",pi->num_progs);
	} else {
		debug_printf("done_command: initializing\n");
	}
	pi->progs = xrealloc(pi->progs, sizeof(*pi->progs) * (pi->num_progs+1));

	prog = pi->progs + pi->num_progs;
	prog->redirects = NULL;
	prog->argv = NULL;
	prog->is_stopped = 0;
	prog->group = NULL;
	prog->glob_result.gl_pathv = NULL;
	prog->family = pi;

	ctx->child=prog;
	/* but ctx->pipe and ctx->list_head remain unchanged */
	return 0;
}

static int done_pipe(struct p_context *ctx, pipe_style type)
{
	struct pipe *new_p;
	done_command(ctx);  /* implicit closure of previous command */
	debug_printf("done_pipe, type %d\n", type);
	ctx->pipe->followup = type;
	ctx->pipe->r_mode = ctx->w;
	new_p=new_pipe();
	ctx->pipe->next = new_p;
	ctx->pipe = new_p;
	ctx->child = NULL;
	done_command(ctx);  /* set up new pipe to accept commands */
	return 0;
}

/* peek ahead in the in_str to find out if we have a "&n" construct,
 * as in "2>&1", that represents duplicating a file descriptor.
 * returns either -2 (syntax error), -1 (no &), or the number found.
 */
static int redirect_dup_num(struct in_str *input)
{
	int ch, d=0, ok=0;
	ch = b_peek(input);
	if (ch != '&') return -1;

	b_getch(input);  /* get the & */
	while (ch=b_peek(input),isdigit(ch)) {
		d = d*10+(ch-'0');
		ok=1;
		b_getch(input);
	}
	if (ok) return d;

	fprintf(stderr, "ambiguous redirect\n");
	return -2;
}

/* If a redirect is immediately preceded by a number, that number is
 * supposed to tell which file descriptor to redirect.  This routine
 * looks for such preceding numbers.  In an ideal world this routine
 * needs to handle all the following classes of redirects...
 *     echo 2>foo     # redirects fd  2 to file "foo", nothing passed to echo
 *     echo 49>foo    # redirects fd 49 to file "foo", nothing passed to echo
 *     echo -2>foo    # redirects fd  1 to file "foo",    "-2" passed to echo
 *     echo 49x>foo   # redirects fd  1 to file "foo",   "49x" passed to echo
 * A -1 output from this program means no valid number was found, so the
 * caller should use the appropriate default for this redirection.
 */
static int redirect_opt_num(o_string *o)
{
	int num;

	if (o->length==0) return -1;
	for(num=0; num<o->length; num++) {
		if (!isdigit(*(o->data+num))) {
			return -1;
		}
	}
	/* reuse num (and save an int) */
	num=atoi(o->data);
	b_reset(o);
	return num;
}

FILE *generate_stream_from_list(struct pipe *head)
{
	FILE *pf;
#if 1
	int pid, channel[2];
	if (pipe(channel)<0) perror_msg_and_die("pipe");
	pid=fork();
	if (pid<0) {
		perror_msg_and_die("fork");
	} else if (pid==0) {
		close(channel[0]);
		if (channel[1] != 1) {
			dup2(channel[1],1);
			close(channel[1]);
		}
#if 0
#define SURROGATE "surrogate response"
		write(1,SURROGATE,sizeof(SURROGATE));
		exit(run_list(head));
#else
		exit(run_list_real(head));   /* leaks memory */
#endif
	}
	debug_printf("forked child %d\n",pid);
	close(channel[1]);
	pf = fdopen(channel[0],"r");
	debug_printf("pipe on FILE *%p\n",pf);
#else
	run_list_test(head,0);
	pf=popen("echo surrogate response","r");
	debug_printf("started fake pipe on FILE *%p\n",pf);
#endif
	return pf;
}

/* this version hacked for testing purposes */
/* return code is exit status of the process that is run. */
static int process_command_subs(o_string *dest, struct p_context *ctx, struct in_str *input, int subst_end)
{
	int retcode;
	o_string result=NULL_O_STRING;
	struct p_context inner;
	FILE *p;
	struct in_str pipe_str;
	initialize_context(&inner);

	/* recursion to generate command */
	retcode = parse_stream(&result, &inner, input, subst_end);
	if (retcode != 0) return retcode;  /* syntax error or EOF */
	done_word(&result, &inner);
	done_pipe(&inner, PIPE_SEQ);
	b_free(&result);

	p=generate_stream_from_list(inner.list_head);
	if (p==NULL) return 1;
	mark_open(fileno(p));
	setup_file_in_str(&pipe_str, p);

	/* now send results of command back into original context */
	retcode = parse_stream(dest, ctx, &pipe_str, '\0');
	/* XXX In case of a syntax error, should we try to kill the child?
	 * That would be tough to do right, so just read until EOF. */
	if (retcode == 1) {
		while (b_getch(&pipe_str)!=EOF) { /* discard */ };
	}

	debug_printf("done reading from pipe, pclose()ing\n");
	/* This is the step that wait()s for the child.  Should be pretty
	 * safe, since we just read an EOF from its stdout.  We could try
	 * to better, by using wait(), and keeping track of background jobs
	 * at the same time.  That would be a lot of work, and contrary
	 * to the KISS philosophy of this program. */
	mark_closed(fileno(p));
	retcode=pclose(p);
	debug_printf("pclosed, retcode=%d\n",retcode);
	/* XXX this process fails to trim a single trailing newline */
	return retcode;
}

static int parse_group(o_string *dest, struct p_context *ctx,
	struct in_str *input, int ch)
{
	int rcode, endch=0;
	struct p_context sub;
	struct child_prog *child = ctx->child;
	if (child->argv) {
		syntax();
		return 1;  /* syntax error, groups and arglists don't mix */
	}
	initialize_context(&sub);
	switch(ch) {
		case '(': endch=')'; child->subshell=1; break;
		case '{': endch='}'; break;
		default: syntax();   /* really logic error */
	}
	rcode=parse_stream(dest,&sub,input,endch);
	done_word(dest,&sub); /* finish off the final word in the subcontext */
	done_pipe(&sub, PIPE_SEQ);  /* and the final command there, too */
	child->group = sub.list_head;
	return rcode;
	/* child remains "open", available for possible redirects */
}

/* basically useful version until someone wants to get fancier,
 * see the bash man page under "Parameter Expansion" */
static void lookup_param(o_string *dest, struct p_context *ctx, o_string *src)
{
	const char *p=NULL;
	if (src->data) p = getenv(src->data);
	if (p) parse_string(dest, ctx, p);   /* recursion */
	b_free(src);
}

/* return code: 0 for OK, 1 for syntax error */
static int handle_dollar(o_string *dest, struct p_context *ctx, struct in_str *input)
{
	int i, advance=0;
	o_string alt=NULL_O_STRING;
	char sep[]=" ";
	int ch = input->peek(input);  /* first character after the $ */
	debug_printf("handle_dollar: ch=%c\n",ch);
	if (isalpha(ch)) {
		while(ch=b_peek(input),isalnum(ch) || ch=='_') {
			b_getch(input);
			b_addchr(&alt,ch);
		}
		lookup_param(dest, ctx, &alt);
	} else if (isdigit(ch)) {
		i = ch-'0';  /* XXX is $0 special? */
		if (i<global_argc) {
			parse_string(dest, ctx, global_argv[i]); /* recursion */
		}
		advance = 1;
	} else switch (ch) {
		case '$':
			b_adduint(dest,getpid());
			advance = 1;
			break;
		case '!':
			if (last_bg_pid > 0) b_adduint(dest, last_bg_pid);
			advance = 1;
			break;
		case '?':
			b_adduint(dest,last_return_code);
			advance = 1;
			break;
		case '#':
			b_adduint(dest,global_argc ? global_argc-1 : 0);
			advance = 1;
			break;
		case '{':
			b_getch(input);
			/* XXX maybe someone will try to escape the '}' */
			while(ch=b_getch(input),ch!=EOF && ch!='}') {
				b_addchr(&alt,ch);
			}
			if (ch != '}') {
				syntax();
				return 1;
			}
			lookup_param(dest, ctx, &alt);
			break;
		case '(':
			process_command_subs(dest, ctx, input, ')');
			break;
		case '*':
			sep[0]=ifs[0];
			for (i=1; i<global_argc; i++) {
				parse_string(dest, ctx, global_argv[i]);
				if (i+1 < global_argc) parse_string(dest, ctx, sep);
			}
			break;
		case '@':
		case '-':
		case '_':
			/* still unhandled, but should be eventually */
			fprintf(stderr,"unhandled syntax: $%c\n",ch);
			return 1;
			break;
		default:
			b_addqchr(dest,'$',dest->quote);
	}
	/* Eat the character if the flag was set.  If the compiler
	 * is smart enough, we could substitute "b_getch(input);"
	 * for all the "advance = 1;" above, and also end up with
	 * a nice size-optimized program.  Hah!  That'll be the day.
	 */
	if (advance) b_getch(input);
	return 0;
}

int parse_string(o_string *dest, struct p_context *ctx, const char *src)
{
	struct in_str foo;
	setup_string_in_str(&foo, src);
	return parse_stream(dest, ctx, &foo, '\0');
}

/* return code is 0 for normal exit, 1 for syntax error */
int parse_stream(o_string *dest, struct p_context *ctx,
	struct in_str *input, int end_trigger)
{
	unsigned int ch, m;
	int redir_fd;
	redir_type redir_style;
	int next;

	/* Only double-quote state is handled in the state variable dest->quote.
	 * A single-quote triggers a bypass of the main loop until its mate is
	 * found.  When recursing, quote state is passed in via dest->quote. */

	debug_printf("parse_stream, end_trigger=%d\n",end_trigger);
	while ((ch=b_getch(input))!=EOF) {
		m = map[ch];
		next = (ch == '\n') ? 0 : b_peek(input);
		debug_printf("parse_stream: ch=%c (%d) m=%d quote=%d\n",
			ch,ch,m,dest->quote);
		if (m==0 || ((m==1 || m==2) && dest->quote)) {
			b_addqchr(dest, ch, dest->quote);
		} else {
			if (m==2) {  /* unquoted IFS */
				done_word(dest, ctx);
				if (ch=='\n') done_pipe(ctx,PIPE_SEQ);
			}
			if (ch == end_trigger && !dest->quote && ctx->w==RES_NONE) {
				debug_printf("leaving parse_stream\n");
				return 0;
			}
#if 0
			if (ch=='\n') {
				/* Yahoo!  Time to run with it! */
				done_pipe(ctx,PIPE_SEQ);
				run_list(ctx->list_head);
				initialize_context(ctx);
			}
#endif
			if (m!=2) switch (ch) {
		case '#':
			if (dest->length == 0 && !dest->quote) {
				while(ch=b_peek(input),ch!=EOF && ch!='\n') { b_getch(input); }
			} else {
				b_addqchr(dest, ch, dest->quote);
			}
			break;
		case '\\':
			if (next == EOF) {
				syntax();
				return 1;
			}
			b_addqchr(dest, '\\', dest->quote);
			b_addqchr(dest, b_getch(input), dest->quote);
			break;
		case '$':
			if (handle_dollar(dest, ctx, input)!=0) return 1;
			break;
		case '\'':
			dest->nonnull = 1;
			while(ch=b_getch(input),ch!=EOF && ch!='\'') {
				b_addchr(dest,ch);
			}
			if (ch==EOF) {
				syntax();
				return 1;
			}
			break;
		case '"':
			dest->nonnull = 1;
			dest->quote = !dest->quote;
			break;
		case '`':
			process_command_subs(dest, ctx, input, '`');
			break;
		case '>':
			redir_fd = redirect_opt_num(dest);
			done_word(dest, ctx);
			redir_style=REDIRECT_OVERWRITE;
			if (next == '>') {
				redir_style=REDIRECT_APPEND;
				b_getch(input);
			} else if (next == '(') {
				syntax();   /* until we support >(list) Process Substitution */
				return 1;
			}
			setup_redirect(ctx, redir_fd, redir_style, input);
			break;
		case '<':
			redir_fd = redirect_opt_num(dest);
			done_word(dest, ctx);
			redir_style=REDIRECT_INPUT;
			if (next == '<') {
				redir_style=REDIRECT_HEREIS;
				b_getch(input);
			} else if (next == '>') {
				redir_style=REDIRECT_IO;
				b_getch(input);
			} else if (next == '(') {
				syntax();   /* until we support <(list) Process Substitution */
				return 1;
			}
			setup_redirect(ctx, redir_fd, redir_style, input);
			break;
		case ';':
			done_word(dest, ctx);
			done_pipe(ctx,PIPE_SEQ);
			break;
		case '&':
			done_word(dest, ctx);
			if (next=='&') {
				b_getch(input);
				done_pipe(ctx,PIPE_AND);
			} else {
				done_pipe(ctx,PIPE_BG);
			}
			break;
		case '|':
			done_word(dest, ctx);
			if (next=='|') {
				b_getch(input);
				done_pipe(ctx,PIPE_OR);
			} else {
				/* we could pick up a file descriptor choice here
				 * with redirect_opt_num(), but bash doesn't do it.
				 * "echo foo 2| cat" yields "foo 2". */
				done_command(ctx);
			}
			break;
		case '(':
		case '{':
			if (parse_group(dest, ctx, input, ch)!=0) return 1;
			break;
		case ')':
		case '}':
			syntax();   /* Proper use of this character caught by end_trigger */
			return 1;
			break;
		default:
			syntax();   /* this is really an internal logic error */
			return 1;
			}
		}
	}
	/* complain if quote?  No, maybe we just finished a command substitution
	 * that was quoted.  Example:
	 * $ echo "`cat foo` plus more" 
	 * and we just got the EOF generated by the subshell that ran "cat foo"
	 * The only real complaint is if we got an EOF when end_trigger != '\0',
	 * that is, we were really supposed to get end_trigger, and never got
	 * one before the EOF.  Can't use the standard "syntax error" return code,
	 * so that parse_stream_outer can distinguish the EOF and exit smoothly. */
	if (end_trigger != '\0') return -1;
	return 0;
}

void mapset(const unsigned char *set, int code)
{
	const unsigned char *s;
	for (s=set; *s; s++) map[*s] = code;
}

void update_ifs_map(void)
{
	/* char *ifs and char map[256] are both globals. */
	ifs = getenv("IFS");
	if (ifs == NULL) ifs=" \t\n";
	/* Precompute a list of 'flow through' behavior so it can be treated
	 * quickly up front.  Computation is necessary because of IFS.
	 * Special case handling of IFS == " \t\n" is not implemented.
	 * The map[] array only really needs two bits each, and on most machines
	 * that would be faster because of the reduced L1 cache footprint.
	 */
	memset(map,0,256);        /* most characters flow through always */
	mapset("\\$'\"`", 3);     /* never flow through */
	mapset("<>;&|(){}#", 1);  /* flow through if quoted */
	mapset(ifs, 2);           /* also flow through if quoted */
}

/* most recursion does not come through here, the exeception is
 * from builtin_source() */
int parse_stream_outer(struct in_str *inp)
{

	struct p_context ctx;
	o_string temp=NULL_O_STRING;
	int rcode;
	do {
		initialize_context(&ctx);
		update_ifs_map();
		inp->promptmode=1;
		rcode = parse_stream(&temp, &ctx, inp, '\n');
		done_word(&temp, &ctx);
		done_pipe(&ctx,PIPE_SEQ);
		run_list(ctx.list_head);
	} while (rcode != -1);   /* loop on syntax errors, return on EOF */
	return 0;
}

static int parse_string_outer(const char *s)
{
	struct in_str input;
	setup_string_in_str(&input, s);
	return parse_stream_outer(&input);
}

static int parse_file_outer(FILE *f)
{
	int rcode;
	struct in_str input;
	setup_file_in_str(&input, f);
	rcode = parse_stream_outer(&input);
	return rcode;
}

int shell_main(int argc, char **argv)
{
	int opt;
	FILE *input;

	/* XXX what should these be while sourcing /etc/profile? */
	global_argc = argc;
	global_argv = argv;

	if (argv[0] && argv[0][0] == '-') {
		debug_printf("\nsourcing /etc/profile\n");
		input = xfopen("/etc/profile", "r");
		mark_open(fileno(input));
		parse_file_outer(input);
		mark_closed(fileno(input));
		fclose(input);
	}
	input=stdin;
	
	/* initialize the cwd -- this is never freed...*/
	cwd = xgetcwd(0);
#ifdef BB_FEATURE_COMMAND_EDITING
	cmdedit_set_initial_prompt();
#else
	PS1 = NULL;
#endif
	
	while ((opt = getopt(argc, argv, "c:xif")) > 0) {
		switch (opt) {
			case 'c':
				{
					global_argv = argv+optind;
					global_argc = argc-optind;
					opt = parse_string_outer(optarg);
					exit(opt);
				}
				break;
			case 'i':
				interactive++;
				break;
			case 'f':
				fake_mode++;
				break;
			default:
				fprintf(stderr, "Usage: sh [FILE]...\n"
						"   or: sh -c command [args]...\n\n");
				exit(EXIT_FAILURE);
		}
	}
	/* A shell is interactive if the `-i' flag was given, or if all of
	 * the following conditions are met:
	 *	  no -c command
	 *    no arguments remaining or the -s flag given
	 *    standard input is a terminal
	 *    standard output is a terminal
	 *    Refer to Posix.2, the description of the `sh' utility. */
	if (argv[optind]==NULL && input==stdin &&
			isatty(fileno(stdin)) && isatty(fileno(stdout))) {
		interactive++;
	}
	
	if (interactive) {
		/* Looks like they want an interactive shell */
		fprintf(stdout, "\nhush -- the humble shell v0.01 (testing)\n\n");
		exit(parse_file_outer(stdin));
	}
	debug_printf("\ninteractive=%d\n", interactive);

	debug_printf("\nrunning script '%s'\n", argv[optind]);
	global_argv = argv+optind;
	global_argc = argc-optind;
	input = xfopen(argv[optind], "r");
	opt = parse_file_outer(input);

#ifdef BB_FEATURE_CLEAN_UP
	fclose(input.file);
#endif

	return(opt);
}
