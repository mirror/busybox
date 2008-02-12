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
 *      written Dec 2000 and Jan 2001 by Larry Doolittle.  The
 *      execution engine, the builtins, and much of the underlying
 *      support has been adapted from busybox-0.49pre's lash, which is
 *      Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *      written by Erik Andersen <andersen@codepoet.org>.  That, in turn,
 *      is based in part on ladsh.c, by Michael K. Johnson and Erik W.
 *      Troan, which they placed in the public domain.  I don't know
 *      how much of the Johnson/Troan code has survived the repeated
 *      rewrites.
 *
 * Other credits:
 *      b_addchr() derived from similar w_addchar function in glibc-2.2
 *      setup_redirect(), redirect_opt_num(), and big chunks of main()
 *      and many builtins derived from contributions by Erik Andersen
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
 *      $_
 *      ! negation operator for pipes
 *      &> and >& redirection of stdout+stderr
 *      Brace Expansion
 *      Tilde Expansion
 *      fancy forms of Parameter Expansion
 *      aliases
 *      Arithmetic Expansion
 *      <(list) and >(list) Process Substitution
 *      reserved words: case, esac, select, function
 *      Here Documents ( << word )
 *      Functions
 * Major bugs:
 *      job handling woefully incomplete and buggy (improved --vda)
 *      reserved word execution woefully incomplete and buggy
 * to-do:
 *      port selected bugfixes from post-0.49 busybox lash - done?
 *      finish implementing reserved words: for, while, until, do, done
 *      change { and } from special chars to reserved words
 *      builtins: break, continue, eval, return, set, trap, ulimit
 *      test magic exec
 *      handle children going into background
 *      clean up recognition of null pipes
 *      check setting of global_argc and global_argv
 *      control-C handling, probably with longjmp
 *      follow IFS rules more precisely, including update semantics
 *      figure out what to do with backslash-newline
 *      explain why we use signal instead of sigaction
 *      propagate syntax errors, die on resource errors?
 *      continuation lines, both explicit and implicit - done?
 *      memory leak finding and plugging - done?
 *      more testing, especially quoting rules and redirection
 *      document how quoting rules not precisely followed for variable assignments
 *      maybe change charmap[] to use 2-bit entries
 *      (eventually) remove all the printf's
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */


#include <glob.h>      /* glob, of course */
#include <getopt.h>    /* should be pretty obvious */
/* #include <dmalloc.h> */
extern char **environ;
#include "busybox.h" /* for APPLET_IS_NOFORK/NOEXEC */


#if !BB_MMU && ENABLE_HUSH_TICK
//#undef ENABLE_HUSH_TICK
//#define ENABLE_HUSH_TICK 0
#warning On NOMMU, hush command substitution is dangerous.
#warning Dont use it for commands which produce lots of output.
#warning For more info see shell/hush.c, generate_stream_from_list().
#endif

#if !BB_MMU && ENABLE_HUSH_JOB
#undef ENABLE_HUSH_JOB
#define ENABLE_HUSH_JOB 0
#endif

#if !ENABLE_HUSH_INTERACTIVE
#undef ENABLE_FEATURE_EDITING
#define ENABLE_FEATURE_EDITING 0
#undef ENABLE_FEATURE_EDITING_FANCY_PROMPT
#define ENABLE_FEATURE_EDITING_FANCY_PROMPT 0
#endif


/* If you comment out one of these below, it will be #defined later
 * to perform debug printfs to stderr: */
#define debug_printf(...)        do {} while (0)
/* Finer-grained debug switches */
#define debug_printf_parse(...)  do {} while (0)
#define debug_print_tree(a, b)   do {} while (0)
#define debug_printf_exec(...)   do {} while (0)
#define debug_printf_jobs(...)   do {} while (0)
#define debug_printf_expand(...) do {} while (0)
#define debug_printf_clean(...)  do {} while (0)

#ifndef debug_printf
#define debug_printf(...) fprintf(stderr, __VA_ARGS__)
#endif

#ifndef debug_printf_parse
#define debug_printf_parse(...) fprintf(stderr, __VA_ARGS__)
#endif

#ifndef debug_printf_exec
#define debug_printf_exec(...) fprintf(stderr, __VA_ARGS__)
#endif

#ifndef debug_printf_jobs
#define debug_printf_jobs(...) fprintf(stderr, __VA_ARGS__)
#define DEBUG_SHELL_JOBS 1
#endif

#ifndef debug_printf_expand
#define debug_printf_expand(...) fprintf(stderr, __VA_ARGS__)
#define DEBUG_EXPAND 1
#endif

/* Keep unconditionally on for now */
#define ENABLE_HUSH_DEBUG 1

#ifndef debug_printf_clean
/* broken, of course, but OK for testing */
static const char *indenter(int i)
{
	static const char blanks[] ALIGN1 =
		"                                    ";
	return &blanks[sizeof(blanks) - i - 1];
}
#define debug_printf_clean(...) fprintf(stderr, __VA_ARGS__)
#define DEBUG_CLEAN 1
#endif


/*
 * Leak hunting. Use hush_leaktool.sh for post-processing.
 */
#ifdef FOR_HUSH_LEAKTOOL
void *xxmalloc(int lineno, size_t size)
{
	void *ptr = xmalloc((size + 0xff) & ~0xff);
	fprintf(stderr, "line %d: malloc %p\n", lineno, ptr);
	return ptr;
}
void *xxrealloc(int lineno, void *ptr, size_t size)
{
	ptr = xrealloc(ptr, (size + 0xff) & ~0xff);
	fprintf(stderr, "line %d: realloc %p\n", lineno, ptr);
	return ptr;
}
char *xxstrdup(int lineno, const char *str)
{
	char *ptr = xstrdup(str);
	fprintf(stderr, "line %d: strdup %p\n", lineno, ptr);
	return ptr;
}
void xxfree(void *ptr)
{
	fprintf(stderr, "free %p\n", ptr);
	free(ptr);
}
#define xmalloc(s)     xxmalloc(__LINE__, s)
#define xrealloc(p, s) xxrealloc(__LINE__, p, s)
#define xstrdup(s)     xxstrdup(__LINE__, s)
#define free(p)        xxfree(p)
#endif


#define SPECIAL_VAR_SYMBOL   3

#define PARSEFLAG_EXIT_FROM_LOOP 1
#define PARSEFLAG_SEMICOLON      (1 << 1)  /* symbol ';' is special for parser */
#define PARSEFLAG_REPARSING      (1 << 2)  /* >= 2nd pass */

typedef enum {
	REDIRECT_INPUT     = 1,
	REDIRECT_OVERWRITE = 2,
	REDIRECT_APPEND    = 3,
	REDIRECT_HEREIS    = 4,
	REDIRECT_IO        = 5
} redir_type;

/* The descrip member of this structure is only used to make debugging
 * output pretty */
static const struct {
	int mode;
	signed char default_fd;
	char descrip[3];
} redir_table[] = {
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
#if ENABLE_HUSH_IF
	RES_IF    = 1,
	RES_THEN  = 2,
	RES_ELIF  = 3,
	RES_ELSE  = 4,
	RES_FI    = 5,
#endif
#if ENABLE_HUSH_LOOPS
	RES_FOR   = 6,
	RES_WHILE = 7,
	RES_UNTIL = 8,
	RES_DO    = 9,
	RES_DONE  = 10,
	RES_IN    = 11,
#endif
	RES_XXXX  = 12,
	RES_SNTX  = 13
} reserved_style;
enum {
	FLAG_END   = (1 << RES_NONE ),
#if ENABLE_HUSH_IF
	FLAG_IF    = (1 << RES_IF   ),
	FLAG_THEN  = (1 << RES_THEN ),
	FLAG_ELIF  = (1 << RES_ELIF ),
	FLAG_ELSE  = (1 << RES_ELSE ),
	FLAG_FI    = (1 << RES_FI   ),
#endif
#if ENABLE_HUSH_LOOPS
	FLAG_FOR   = (1 << RES_FOR  ),
	FLAG_WHILE = (1 << RES_WHILE),
	FLAG_UNTIL = (1 << RES_UNTIL),
	FLAG_DO    = (1 << RES_DO   ),
	FLAG_DONE  = (1 << RES_DONE ),
	FLAG_IN    = (1 << RES_IN   ),
#endif
	FLAG_START = (1 << RES_XXXX ),
};

/* This holds pointers to the various results of parsing */
struct p_context {
	struct child_prog *child;
	struct pipe *list_head;
	struct pipe *pipe;
	struct redir_struct *pending_redirect;
	smallint res_w;
	smallint parse_type;        /* bitmask of PARSEFLAG_xxx, defines type of parser : ";$" common or special symbol */
	int old_flag;               /* bitmask of FLAG_xxx, for figuring out valid reserved words */
	struct p_context *stack;
	/* How about quoting status? */
};

struct redir_struct {
	struct redir_struct *next;  /* pointer to the next redirect in the list */
	redir_type type;            /* type of redirection */
	int fd;                     /* file descriptor being redirected */
	int dup;                    /* -1, or file descriptor being duplicated */
	char **glob_word;           /* *word.gl_pathv is the filename */
};

struct child_prog {
	pid_t pid;                  /* 0 if exited */
	char **argv;                /* program name and arguments */
	struct pipe *group;         /* if non-NULL, first in group or subshell */
	smallint subshell;          /* flag, non-zero if group must be forked */
	smallint is_stopped;        /* is the program currently running? */
	struct redir_struct *redirects; /* I/O redirections */
	struct pipe *family;        /* pointer back to the child's parent pipe */
	//sp counting seems to be broken... so commented out, grep for '//sp:'
	//sp: int sp;               /* number of SPECIAL_VAR_SYMBOL */
	//seems to be unused, grep for '//pt:'
	//pt: int parse_type;
};
/* argv vector may contain variable references (^Cvar^C, ^C0^C etc)
 * and on execution these are substituted with their values.
 * Substitution can make _several_ words out of one argv[n]!
 * Example: argv[0]=='.^C*^C.' here: echo .$*.
 */

struct pipe {
	struct pipe *next;
	int num_progs;              /* total number of programs in job */
	int running_progs;          /* number of programs running (not exited) */
	int stopped_progs;          /* number of programs alive, but stopped */
#if ENABLE_HUSH_JOB
	int jobid;                  /* job number */
	pid_t pgrp;                 /* process group ID for the job */
	char *cmdtext;              /* name of job */
#endif
	char *cmdbuf;               /* buffer various argv's point into */
	struct child_prog *progs;   /* array of commands in pipe */
	int job_context;            /* bitmask defining current context */
	smallint followup;          /* PIPE_BG, PIPE_SEQ, PIPE_OR, PIPE_AND */
	smallint res_word;          /* needed for if, for, while, until... */
};

/* On program start, environ points to initial environment.
 * putenv adds new pointers into it, unsetenv removes them.
 * Neither of these (de)allocates the strings.
 * setenv allocates new strings in malloc space and does putenv,
 * and thus setenv is unusable (leaky) for shell's purposes */
#define setenv(...) setenv_is_leaky_dont_use()
struct variable {
	struct variable *next;
	char *varstr;        /* points to "name=" portion */
	int max_len;         /* if > 0, name is part of initial env; else name is malloced */
	smallint flg_export; /* putenv should be done on this var */
	smallint flg_read_only;
};

typedef struct {
	char *data;
	int length;
	int maxlen;
	smallint o_quote;
	smallint nonnull;
} o_string;
#define NULL_O_STRING {NULL,0,0,0,0}
/* used for initialization: o_string foo = NULL_O_STRING; */

/* I can almost use ordinary FILE *.  Is open_memstream() universally
 * available?  Where is it documented? */
struct in_str {
	const char *p;
	/* eof_flag=1: last char in ->p is really an EOF */
	char eof_flag; /* meaningless if ->p == NULL */
	char peek_buf[2];
#if ENABLE_HUSH_INTERACTIVE
	smallint promptme;
	smallint promptmode; /* 0: PS1, 1: PS2 */
#endif
	FILE *file;
	int (*get) (struct in_str *);
	int (*peek) (struct in_str *);
};
#define b_getch(input) ((input)->get(input))
#define b_peek(input) ((input)->peek(input))

enum {
	CHAR_ORDINARY           = 0,
	CHAR_ORDINARY_IF_QUOTED = 1, /* example: *, # */
	CHAR_IFS                = 2, /* treated as ordinary if quoted */
	CHAR_SPECIAL            = 3, /* example: $ */
};

#define HUSH_VER_STR "0.02"

/* "Globals" within this file */

/* Sorted roughly by size (smaller offsets == smaller code) */
struct globals {
#if ENABLE_HUSH_INTERACTIVE
	/* 'interactive_fd' is a fd# open to ctty, if we have one
	 * _AND_ if we decided to act interactively */
	int interactive_fd;
	const char *PS1;
	const char *PS2;
#endif
#if ENABLE_FEATURE_EDITING
	line_input_t *line_input_state;
#endif
#if ENABLE_HUSH_JOB
	int run_list_level;
	pid_t saved_task_pgrp;
	pid_t saved_tty_pgrp;
	int last_jobid;
	struct pipe *job_list;
	struct pipe *toplevel_list;
	smallint ctrl_z_flag;
#endif
	smallint fake_mode;
	/* these three support $?, $#, and $1 */
	char **global_argv;
	int global_argc;
	int last_return_code;
	const char *ifs;
	const char *cwd;
	unsigned last_bg_pid;
	struct variable *top_var; /* = &shell_ver (set in main()) */
	struct variable shell_ver;
#if ENABLE_FEATURE_SH_STANDALONE
	struct nofork_save_area nofork_save;
#endif
#if ENABLE_HUSH_JOB
	sigjmp_buf toplevel_jb;
#endif
	unsigned char charmap[256];
	char user_input_buf[ENABLE_FEATURE_EDITING ? BUFSIZ : 2];
};

#define G (*ptr_to_globals)

#if !ENABLE_HUSH_INTERACTIVE
enum { interactive_fd = 0 };
#endif
#if !ENABLE_HUSH_JOB
enum { run_list_level = 0 };
#endif

#if ENABLE_HUSH_INTERACTIVE
#define interactive_fd   (G.interactive_fd  )
#define PS1              (G.PS1             )
#define PS2              (G.PS2             )
#endif
#if ENABLE_FEATURE_EDITING
#define line_input_state (G.line_input_state)
#endif
#if ENABLE_HUSH_JOB
#define run_list_level   (G.run_list_level  )
#define saved_task_pgrp  (G.saved_task_pgrp )
#define saved_tty_pgrp   (G.saved_tty_pgrp  )
#define last_jobid       (G.last_jobid      )
#define job_list         (G.job_list        )
#define toplevel_list    (G.toplevel_list   )
#define toplevel_jb      (G.toplevel_jb     )
#define ctrl_z_flag      (G.ctrl_z_flag     )
#endif /* JOB */
#define global_argv      (G.global_argv     )
#define global_argc      (G.global_argc     )
#define last_return_code (G.last_return_code)
#define ifs              (G.ifs             )
#define fake_mode        (G.fake_mode       )
#define cwd              (G.cwd             )
#define last_bg_pid      (G.last_bg_pid     )
#define top_var          (G.top_var         )
#define shell_ver        (G.shell_ver       )
#if ENABLE_FEATURE_SH_STANDALONE
#define nofork_save      (G.nofork_save     )
#endif
#if ENABLE_HUSH_JOB
#define toplevel_jb      (G.toplevel_jb     )
#endif
#define charmap          (G.charmap         )
#define user_input_buf   (G.user_input_buf  )


#define B_CHUNK  100
#define B_NOSPAC 1
#define JOB_STATUS_FORMAT "[%d] %-22s %.40s\n"

#if 1
/* Normal */
static void syntax(const char *msg)
{
	/* Was using fancy stuff:
	 * (interactive_fd ? bb_error_msg : bb_error_msg_and_die)(...params...)
	 * but it SEGVs. ?! Oh well... explicit temp ptr works around that */
	void (*fp)(const char *s, ...);

	fp = (interactive_fd ? bb_error_msg : bb_error_msg_and_die);
	fp(msg ? "%s: %s" : "syntax error", "syntax error", msg);
}

#else
/* Debug */
static void syntax_lineno(int line)
{
	void (*fp)(const char *s, ...);

	fp = (interactive_fd ? bb_error_msg : bb_error_msg_and_die);
	fp("syntax error hush.c:%d", line);
}
#define syntax(str) syntax_lineno(__LINE__)
#endif

/* Index of subroutines: */
/*   o_string manipulation: */
static int b_check_space(o_string *o, int len);
static int b_addchr(o_string *o, int ch);
static void b_reset(o_string *o);
static int b_addqchr(o_string *o, int ch, int quote);
/*  in_str manipulations: */
static int static_get(struct in_str *i);
static int static_peek(struct in_str *i);
static int file_get(struct in_str *i);
static int file_peek(struct in_str *i);
static void setup_file_in_str(struct in_str *i, FILE *f);
static void setup_string_in_str(struct in_str *i, const char *s);
/*  "run" the final data structures: */
#if !defined(DEBUG_CLEAN)
#define free_pipe_list(head, indent) free_pipe_list(head)
#define free_pipe(pi, indent)        free_pipe(pi)
#endif
static int free_pipe_list(struct pipe *head, int indent);
static int free_pipe(struct pipe *pi, int indent);
/*  really run the final data structures: */
static int setup_redirects(struct child_prog *prog, int squirrel[]);
static int run_list(struct pipe *pi);
static void pseudo_exec_argv(char **argv) ATTRIBUTE_NORETURN;
static void pseudo_exec(struct child_prog *child) ATTRIBUTE_NORETURN;
static int run_pipe(struct pipe *pi);
/*   extended glob support: */
static char **globhack(const char *src, char **strings);
static int glob_needed(const char *s);
static int xglob(o_string *dest, char ***pglob);
/*   variable assignment: */
static int is_assignment(const char *s);
/*   data structure manipulation: */
static int setup_redirect(struct p_context *ctx, int fd, redir_type style, struct in_str *input);
static void initialize_context(struct p_context *ctx);
static int done_word(o_string *dest, struct p_context *ctx);
static int done_command(struct p_context *ctx);
static int done_pipe(struct p_context *ctx, pipe_style type);
/*   primary string parsing: */
static int redirect_dup_num(struct in_str *input);
static int redirect_opt_num(o_string *o);
#if ENABLE_HUSH_TICK
static int process_command_subs(o_string *dest, struct p_context *ctx, struct in_str *input, const char *subst_end);
#endif
static int parse_group(o_string *dest, struct p_context *ctx, struct in_str *input, int ch);
static const char *lookup_param(const char *src);
static int handle_dollar(o_string *dest, struct p_context *ctx, struct in_str *input);
static int parse_stream(o_string *dest, struct p_context *ctx, struct in_str *input0, const char *end_trigger);
/*   setup: */
static int parse_and_run_stream(struct in_str *inp, int parse_flag);
static int parse_and_run_string(const char *s, int parse_flag);
static int parse_and_run_file(FILE *f);
/*   job management: */
static int checkjobs(struct pipe* fg_pipe);
#if ENABLE_HUSH_JOB
static int checkjobs_and_fg_shell(struct pipe* fg_pipe);
static void insert_bg_job(struct pipe *pi);
static void remove_bg_job(struct pipe *pi);
static void delete_finished_bg_job(struct pipe *pi);
#else
int checkjobs_and_fg_shell(struct pipe* fg_pipe); /* never called */
#endif
/*     local variable support */
static char **expand_strvec_to_strvec(char **argv);
/* used for eval */
static char *expand_strvec_to_string(char **argv);
/* used for expansion of right hand of assignments */
static char *expand_string_to_string(const char *str);
static struct variable *get_local_var(const char *name);
static int set_local_var(char *str, int flg_export);
static void unset_local_var(const char *name);


static char **add_strings_to_strings(int need_xstrdup, char **strings, char **add)
{
	int i;
	unsigned count1;
	unsigned count2;
	char **v;

	v = strings;
	count1 = 0;
	if (v) {
		while (*v) {
			count1++;
			v++;
		}
	}
	count2 = 0;
	v = add;
	while (*v) {
		count2++;
		v++;
	}
	v = xrealloc(strings, (count1 + count2 + 1) * sizeof(char*));
	v[count1 + count2] = NULL;
	i = count2;
	while (--i >= 0)
		v[count1 + i] = need_xstrdup ? xstrdup(add[i]) : add[i];
	return v;
}

/* 'add' should be a malloced pointer */
static char **add_string_to_strings(char **strings, char *add)
{
	char *v[2];

	v[0] = add;
	v[1] = NULL;

	return add_strings_to_strings(0, strings, v);
}

static void free_strings(char **strings)
{
	if (strings) {
		char **v = strings;
		while (*v)
			free(*v++);
		free(strings);
	}
}


/* Function prototypes for builtins */
static int builtin_cd(char **argv);
static int builtin_echo(char **argv);
static int builtin_eval(char **argv);
static int builtin_exec(char **argv);
static int builtin_exit(char **argv);
static int builtin_export(char **argv);
#if ENABLE_HUSH_JOB
static int builtin_fg_bg(char **argv);
static int builtin_jobs(char **argv);
#endif
#if ENABLE_HUSH_HELP
static int builtin_help(char **argv);
#endif
static int builtin_pwd(char **argv);
static int builtin_read(char **argv);
static int builtin_test(char **argv);
static int builtin_set(char **argv);
static int builtin_shift(char **argv);
static int builtin_source(char **argv);
static int builtin_umask(char **argv);
static int builtin_unset(char **argv);
//static int builtin_not_written(char **argv);

/* Table of built-in functions.  They can be forked or not, depending on
 * context: within pipes, they fork.  As simple commands, they do not.
 * When used in non-forking context, they can change global variables
 * in the parent shell process.  If forked, of course they cannot.
 * For example, 'unset foo | whatever' will parse and run, but foo will
 * still be set at the end. */
struct built_in_command {
	const char *cmd;                /* name */
	int (*function) (char **argv);  /* function ptr */
#if ENABLE_HUSH_HELP
	const char *descr;              /* description */
#define BLTIN(cmd, func, help) { cmd, func, help }
#else
#define BLTIN(cmd, func, help) { cmd, func }
#endif
};

/* For now, echo and test are unconditionally enabled.
 * Maybe make it configurable? */
static const struct built_in_command bltins[] = {
	BLTIN("["     , builtin_test, "Test condition"),
	BLTIN("[["    , builtin_test, "Test condition"),
#if ENABLE_HUSH_JOB
	BLTIN("bg"    , builtin_fg_bg, "Resume a job in the background"),
#endif
//	BLTIN("break" , builtin_not_written, "Exit for, while or until loop"),
	BLTIN("cd"    , builtin_cd, "Change working directory"),
//	BLTIN("continue", builtin_not_written, "Continue for, while or until loop"),
	BLTIN("echo"  , builtin_echo, "Write strings to stdout"),
	BLTIN("eval"  , builtin_eval, "Construct and run shell command"),
	BLTIN("exec"  , builtin_exec, "Exec command, replacing this shell with the exec'd process"),
	BLTIN("exit"  , builtin_exit, "Exit from shell"),
	BLTIN("export", builtin_export, "Set environment variable"),
#if ENABLE_HUSH_JOB
	BLTIN("fg"    , builtin_fg_bg, "Bring job into the foreground"),
	BLTIN("jobs"  , builtin_jobs, "Lists the active jobs"),
#endif
// TODO: remove pwd? we have it as an applet...
	BLTIN("pwd"   , builtin_pwd, "Print current directory"),
	BLTIN("read"  , builtin_read, "Input environment variable"),
//	BLTIN("return", builtin_not_written, "Return from a function"),
	BLTIN("set"   , builtin_set, "Set/unset shell local variables"),
	BLTIN("shift" , builtin_shift, "Shift positional parameters"),
//	BLTIN("trap"  , builtin_not_written, "Trap signals"),
	BLTIN("test"  , builtin_test, "Test condition"),
//	BLTIN("ulimit", builtin_not_written, "Controls resource limits"),
	BLTIN("umask" , builtin_umask, "Sets file creation mask"),
	BLTIN("unset" , builtin_unset, "Unset environment variable"),
	BLTIN("."     , builtin_source, "Source-in and run commands in a file"),
#if ENABLE_HUSH_HELP
	BLTIN("help"  , builtin_help, "List shell built-in commands"),
#endif
	BLTIN(NULL, NULL, NULL)
};

#if ENABLE_HUSH_JOB

/* move to libbb? */
static void signal_SA_RESTART(int sig, void (*handler)(int))
{
	struct sigaction sa;
	sa.sa_handler = handler;
	sa.sa_flags = SA_RESTART;
	sigemptyset(&sa.sa_mask);
	sigaction(sig, &sa, NULL);
}

/* Signals are grouped, we handle them in batches */
static void set_fatal_sighandler(void (*handler)(int))
{
	signal(SIGILL , handler);
	signal(SIGTRAP, handler);
	signal(SIGABRT, handler);
	signal(SIGFPE , handler);
	signal(SIGBUS , handler);
	signal(SIGSEGV, handler);
	/* bash 3.2 seems to handle these just like 'fatal' ones */
	signal(SIGHUP , handler);
	signal(SIGPIPE, handler);
	signal(SIGALRM, handler);
}
static void set_jobctrl_sighandler(void (*handler)(int))
{
	signal(SIGTSTP, handler);
	signal(SIGTTIN, handler);
	signal(SIGTTOU, handler);
}
static void set_misc_sighandler(void (*handler)(int))
{
	signal(SIGINT , handler);
	signal(SIGQUIT, handler);
	signal(SIGTERM, handler);
}
/* SIGCHLD is special and handled separately */

static void set_every_sighandler(void (*handler)(int))
{
	set_fatal_sighandler(handler);
	set_jobctrl_sighandler(handler);
	set_misc_sighandler(handler);
	signal(SIGCHLD, handler);
}

static void handler_ctrl_c(int sig)
{
	debug_printf_jobs("got sig %d\n", sig);
// as usual we can have all kinds of nasty problems with leaked malloc data here
	siglongjmp(toplevel_jb, 1);
}

static void handler_ctrl_z(int sig)
{
	pid_t pid;

	debug_printf_jobs("got tty sig %d in pid %d\n", sig, getpid());
	pid = fork();
	if (pid < 0) /* can't fork. Pretend there was no ctrl-Z */
		return;
	ctrl_z_flag = 1;
	if (!pid) { /* child */
		setpgrp();
		debug_printf_jobs("set pgrp for child %d ok\n", getpid());
		set_every_sighandler(SIG_DFL);
		raise(SIGTSTP); /* resend TSTP so that child will be stopped */
		debug_printf_jobs("returning in child\n");
		/* return to nofork, it will eventually exit now,
		 * not return back to shell */
		return;
	}
	/* parent */
	/* finish filling up pipe info */
	toplevel_list->pgrp = pid; /* child is in its own pgrp */
	toplevel_list->progs[0].pid = pid;
	/* parent needs to longjmp out of running nofork.
	 * we will "return" exitcode 0, with child put in background */
// as usual we can have all kinds of nasty problems with leaked malloc data here
	debug_printf_jobs("siglongjmp in parent\n");
	siglongjmp(toplevel_jb, 1);
}

/* Restores tty foreground process group, and exits.
 * May be called as signal handler for fatal signal
 * (will faithfully resend signal to itself, producing correct exit state)
 * or called directly with -EXITCODE.
 * We also call it if xfunc is exiting. */
static void sigexit(int sig) ATTRIBUTE_NORETURN;
static void sigexit(int sig)
{
	sigset_t block_all;

	/* Disable all signals: job control, SIGPIPE, etc. */
	sigfillset(&block_all);
	sigprocmask(SIG_SETMASK, &block_all, NULL);

	if (interactive_fd)
		tcsetpgrp(interactive_fd, saved_tty_pgrp);

	/* Not a signal, just exit */
	if (sig <= 0)
		_exit(- sig);

	/* Enable only this sig and kill ourself with it */
	signal(sig, SIG_DFL);
	sigdelset(&block_all, sig);
	sigprocmask(SIG_SETMASK, &block_all, NULL);
	raise(sig);
	_exit(1); /* Should not reach it */
}

/* Restores tty foreground process group, and exits. */
static void hush_exit(int exitcode) ATTRIBUTE_NORETURN;
static void hush_exit(int exitcode)
{
	fflush(NULL); /* flush all streams */
	sigexit(- (exitcode & 0xff));
}

#else /* !JOB */

#define set_fatal_sighandler(handler)   ((void)0)
#define set_jobctrl_sighandler(handler) ((void)0)
#define set_misc_sighandler(handler)    ((void)0)
#define hush_exit(e)                    exit(e)

#endif /* JOB */


static const char *set_cwd(void)
{
	if (cwd == bb_msg_unknown)
		cwd = NULL;     /* xrealloc_getcwd_or_warn(arg) calls free(arg)! */
	cwd = xrealloc_getcwd_or_warn((char *)cwd);
	if (!cwd)
		cwd = bb_msg_unknown;
	return cwd;
}


/* built-in 'test' handler */
static int builtin_test(char **argv)
{
	int argc = 0;
	while (*argv) {
		argc++;
		argv++;
	}
	return test_main(argc, argv - argc);
}

/* built-in 'test' handler */
static int builtin_echo(char **argv)
{
	int argc = 0;
	while (*argv) {
		argc++;
		argv++;
	}
	return echo_main(argc, argv - argc);
}

/* built-in 'eval' handler */
static int builtin_eval(char **argv)
{
	int rcode = EXIT_SUCCESS;

	if (argv[1]) {
		char *str = expand_strvec_to_string(argv + 1);
		parse_and_run_string(str, PARSEFLAG_EXIT_FROM_LOOP |
					PARSEFLAG_SEMICOLON);
		free(str);
		rcode = last_return_code;
	}
	return rcode;
}

/* built-in 'cd <path>' handler */
static int builtin_cd(char **argv)
{
	const char *newdir;
	if (argv[1] == NULL) {
		// bash does nothing (exitcode 0) if HOME is ""; if it's unset,
		// bash says "bash: cd: HOME not set" and does nothing (exitcode 1)
		newdir = getenv("HOME") ? : "/";
	} else
		newdir = argv[1];
	if (chdir(newdir)) {
		printf("cd: %s: %s\n", newdir, strerror(errno));
		return EXIT_FAILURE;
	}
	set_cwd();
	return EXIT_SUCCESS;
}

/* built-in 'exec' handler */
static int builtin_exec(char **argv)
{
	if (argv[1] == NULL)
		return EXIT_SUCCESS;   /* Really? */
	pseudo_exec_argv(argv + 1);
	/* never returns */
}

/* built-in 'exit' handler */
static int builtin_exit(char **argv)
{
// TODO: bash does it ONLY on top-level sh exit (+interacive only?)
	//puts("exit"); /* bash does it */
// TODO: warn if we have background jobs: "There are stopped jobs"
// On second consecutive 'exit', exit anyway.

	if (argv[1] == NULL)
		hush_exit(last_return_code);
	/* mimic bash: exit 123abc == exit 255 + error msg */
	xfunc_error_retval = 255;
	/* bash: exit -2 == exit 254, no error msg */
	hush_exit(xatoi(argv[1]) & 0xff);
}

/* built-in 'export VAR=value' handler */
static int builtin_export(char **argv)
{
	const char *value;
	char *name = argv[1];

	if (name == NULL) {
		// TODO:
		// ash emits: export VAR='VAL'
		// bash: declare -x VAR="VAL"
		// (both also escape as needed (quotes, $, etc))
		char **e = environ;
		if (e)
			while (*e)
				puts(*e++);
		return EXIT_SUCCESS;
	}

	value = strchr(name, '=');
	if (!value) {
		/* They are exporting something without a =VALUE */
		struct variable *var;

		var = get_local_var(name);
		if (var) {
			var->flg_export = 1;
			putenv(var->varstr);
		}
		/* bash does not return an error when trying to export
		 * an undefined variable.  Do likewise. */
		return EXIT_SUCCESS;
	}

	set_local_var(xstrdup(name), 1);
	return EXIT_SUCCESS;
}

#if ENABLE_HUSH_JOB
/* built-in 'fg' and 'bg' handler */
static int builtin_fg_bg(char **argv)
{
	int i, jobnum;
	struct pipe *pi;

	if (!interactive_fd)
		return EXIT_FAILURE;
	/* If they gave us no args, assume they want the last backgrounded task */
	if (!argv[1]) {
		for (pi = job_list; pi; pi = pi->next) {
			if (pi->jobid == last_jobid) {
				goto found;
			}
		}
		bb_error_msg("%s: no current job", argv[0]);
		return EXIT_FAILURE;
	}
	if (sscanf(argv[1], "%%%d", &jobnum) != 1) {
		bb_error_msg("%s: bad argument '%s'", argv[0], argv[1]);
		return EXIT_FAILURE;
	}
	for (pi = job_list; pi; pi = pi->next) {
		if (pi->jobid == jobnum) {
			goto found;
		}
	}
	bb_error_msg("%s: %d: no such job", argv[0], jobnum);
	return EXIT_FAILURE;
 found:
	// TODO: bash prints a string representation
	// of job being foregrounded (like "sleep 1 | cat")
	if (*argv[0] == 'f') {
		/* Put the job into the foreground.  */
		tcsetpgrp(interactive_fd, pi->pgrp);
	}

	/* Restart the processes in the job */
	debug_printf_jobs("reviving %d procs, pgrp %d\n", pi->num_progs, pi->pgrp);
	for (i = 0; i < pi->num_progs; i++) {
		debug_printf_jobs("reviving pid %d\n", pi->progs[i].pid);
		pi->progs[i].is_stopped = 0;
	}
	pi->stopped_progs = 0;

	i = kill(- pi->pgrp, SIGCONT);
	if (i < 0) {
		if (errno == ESRCH) {
			delete_finished_bg_job(pi);
			return EXIT_SUCCESS;
		} else {
			bb_perror_msg("kill (SIGCONT)");
		}
	}

	if (*argv[0] == 'f') {
		remove_bg_job(pi);
		return checkjobs_and_fg_shell(pi);
	}
	return EXIT_SUCCESS;
}
#endif

/* built-in 'help' handler */
#if ENABLE_HUSH_HELP
static int builtin_help(char **argv ATTRIBUTE_UNUSED)
{
	const struct built_in_command *x;

	printf("\nBuilt-in commands:\n");
	printf("-------------------\n");
	for (x = bltins; x->cmd; x++) {
		printf("%s\t%s\n", x->cmd, x->descr);
	}
	printf("\n\n");
	return EXIT_SUCCESS;
}
#endif

#if ENABLE_HUSH_JOB
/* built-in 'jobs' handler */
static int builtin_jobs(char **argv ATTRIBUTE_UNUSED)
{
	struct pipe *job;
	const char *status_string;

	for (job = job_list; job; job = job->next) {
		if (job->running_progs == job->stopped_progs)
			status_string = "Stopped";
		else
			status_string = "Running";

		printf(JOB_STATUS_FORMAT, job->jobid, status_string, job->cmdtext);
	}
	return EXIT_SUCCESS;
}
#endif

/* built-in 'pwd' handler */
static int builtin_pwd(char **argv ATTRIBUTE_UNUSED)
{
	puts(set_cwd());
	return EXIT_SUCCESS;
}

/* built-in 'read VAR' handler */
static int builtin_read(char **argv)
{
	char *string;
	const char *name = argv[1] ? argv[1] : "REPLY";

	string = xmalloc_reads(STDIN_FILENO, xasprintf("%s=", name));
	return set_local_var(string, 0);
}

/* built-in 'set [VAR=value]' handler */
static int builtin_set(char **argv)
{
	char *temp = argv[1];
	struct variable *e;

	if (temp == NULL)
		for (e = top_var; e; e = e->next)
			puts(e->varstr);
	else
		set_local_var(xstrdup(temp), 0);

	return EXIT_SUCCESS;
}


/* Built-in 'shift' handler */
static int builtin_shift(char **argv)
{
	int n = 1;
	if (argv[1]) {
		n = atoi(argv[1]);
	}
	if (n >= 0 && n < global_argc) {
		global_argv[n] = global_argv[0];
		global_argc -= n;
		global_argv += n;
		return EXIT_SUCCESS;
	}
	return EXIT_FAILURE;
}

/* Built-in '.' handler (read-in and execute commands from file) */
static int builtin_source(char **argv)
{
	FILE *input;
	int status;

	if (argv[1] == NULL)
		return EXIT_FAILURE;

	/* XXX search through $PATH is missing */
	input = fopen(argv[1], "r");
	if (!input) {
		bb_error_msg("cannot open '%s'", argv[1]);
		return EXIT_FAILURE;
	}
	close_on_exec_on(fileno(input));

	/* Now run the file */
	/* XXX argv and argc are broken; need to save old global_argv
	 * (pointer only is OK!) on this stack frame,
	 * set global_argv=argv+1, recurse, and restore. */
	status = parse_and_run_file(input);
	fclose(input);
	return status;
}

static int builtin_umask(char **argv)
{
	mode_t new_umask;
	const char *arg = argv[1];
	char *end;
	if (arg) {
		new_umask = strtoul(arg, &end, 8);
		if (*end != '\0' || end == arg) {
			return EXIT_FAILURE;
		}
	} else {
		new_umask = umask(0);
		printf("%.3o\n", (unsigned) new_umask);
	}
	umask(new_umask);
	return EXIT_SUCCESS;
}

/* built-in 'unset VAR' handler */
static int builtin_unset(char **argv)
{
	/* bash always returns true */
	unset_local_var(argv[1]);
	return EXIT_SUCCESS;
}

//static int builtin_not_written(char **argv)
//{
//	printf("builtin_%s not written\n", argv[0]);
//	return EXIT_FAILURE;
//}

static int b_check_space(o_string *o, int len)
{
	/* It would be easy to drop a more restrictive policy
	 * in here, such as setting a maximum string length */
	if (o->length + len > o->maxlen) {
		/* assert(data == NULL || o->maxlen != 0); */
		o->maxlen += (2*len > B_CHUNK ? 2*len : B_CHUNK);
		o->data = xrealloc(o->data, 1 + o->maxlen);
	}
	return o->data == NULL;
}

static int b_addchr(o_string *o, int ch)
{
	debug_printf("b_addchr: '%c' o->length=%d o=%p\n", ch, o->length, o);
	if (b_check_space(o, 1))
		return B_NOSPAC;
	o->data[o->length] = ch;
	o->length++;
	o->data[o->length] = '\0';
	return 0;
}

static void b_reset(o_string *o)
{
	o->length = 0;
	o->nonnull = 0;
	if (o->data)
		o->data[0] = '\0';
}

static void b_free(o_string *o)
{
	free(o->data);
	memset(o, 0, sizeof(*o));
}

/* My analysis of quoting semantics tells me that state information
 * is associated with a destination, not a source.
 */
static int b_addqchr(o_string *o, int ch, int quote)
{
	if (quote && strchr("*?[\\", ch)) {
		int rc;
		rc = b_addchr(o, '\\');
		if (rc)
			return rc;
	}
	return b_addchr(o, ch);
}

static int static_get(struct in_str *i)
{
	int ch = *i->p++;
	if (ch == '\0') return EOF;
	return ch;
}

static int static_peek(struct in_str *i)
{
	return *i->p;
}

#if ENABLE_HUSH_INTERACTIVE
#if ENABLE_FEATURE_EDITING
static void cmdedit_set_initial_prompt(void)
{
#if !ENABLE_FEATURE_EDITING_FANCY_PROMPT
	PS1 = NULL;
#else
	PS1 = getenv("PS1");
	if (PS1 == NULL)
		PS1 = "\\w \\$ ";
#endif
}
#endif /* EDITING */

static const char* setup_prompt_string(int promptmode)
{
	const char *prompt_str;
	debug_printf("setup_prompt_string %d ", promptmode);
#if !ENABLE_FEATURE_EDITING_FANCY_PROMPT
	/* Set up the prompt */
	if (promptmode == 0) { /* PS1 */
		free((char*)PS1);
		PS1 = xasprintf("%s %c ", cwd, (geteuid() != 0) ? '$' : '#');
		prompt_str = PS1;
	} else {
		prompt_str = PS2;
	}
#else
	prompt_str = (promptmode == 0) ? PS1 : PS2;
#endif
	debug_printf("result '%s'\n", prompt_str);
	return prompt_str;
}

static void get_user_input(struct in_str *i)
{
	int r;
	const char *prompt_str;

	prompt_str = setup_prompt_string(i->promptmode);
#if ENABLE_FEATURE_EDITING
	/* Enable command line editing only while a command line
	 * is actually being read; otherwise, we'll end up bequeathing
	 * atexit() handlers and other unwanted stuff to our
	 * child processes (rob@sysgo.de) */
	r = read_line_input(prompt_str, user_input_buf, BUFSIZ-1, line_input_state);
	i->eof_flag = (r < 0);
	if (i->eof_flag) { /* EOF/error detected */
		user_input_buf[0] = EOF; /* yes, it will be truncated, it's ok */
		user_input_buf[1] = '\0';
	}
#else
	fputs(prompt_str, stdout);
	fflush(stdout);
	user_input_buf[0] = r = fgetc(i->file);
	/*user_input_buf[1] = '\0'; - already is and never changed */
	i->eof_flag = (r == EOF);
#endif
	i->p = user_input_buf;
}
#endif  /* INTERACTIVE */

/* This is the magic location that prints prompts
 * and gets data back from the user */
static int file_get(struct in_str *i)
{
	int ch;

	/* If there is data waiting, eat it up */
	if (i->p && *i->p) {
#if ENABLE_HUSH_INTERACTIVE
 take_cached:
#endif
		ch = *i->p++;
		if (i->eof_flag && !*i->p)
			ch = EOF;
	} else {
		/* need to double check i->file because we might be doing something
		 * more complicated by now, like sourcing or substituting. */
#if ENABLE_HUSH_INTERACTIVE
		if (interactive_fd && i->promptme && i->file == stdin) {
			do {
				get_user_input(i);
			} while (!*i->p); /* need non-empty line */
			i->promptmode = 1; /* PS2 */
			i->promptme = 0;
			goto take_cached;
		}
#endif
		ch = fgetc(i->file);
	}
	debug_printf("file_get: got a '%c' %d\n", ch, ch);
#if ENABLE_HUSH_INTERACTIVE
	if (ch == '\n')
		i->promptme = 1;
#endif
	return ch;
}

/* All the callers guarantee this routine will never be
 * used right after a newline, so prompting is not needed.
 */
static int file_peek(struct in_str *i)
{
	int ch;
	if (i->p && *i->p) {
		if (i->eof_flag && !i->p[1])
			return EOF;
		return *i->p;
	}
	ch = fgetc(i->file);
	i->eof_flag = (ch == EOF);
	i->peek_buf[0] = ch;
	i->peek_buf[1] = '\0';
	i->p = i->peek_buf;
	debug_printf("file_peek: got a '%c' %d\n", *i->p, *i->p);
	return ch;
}

static void setup_file_in_str(struct in_str *i, FILE *f)
{
	i->peek = file_peek;
	i->get = file_get;
#if ENABLE_HUSH_INTERACTIVE
	i->promptme = 1;
	i->promptmode = 0; /* PS1 */
#endif
	i->file = f;
	i->p = NULL;
}

static void setup_string_in_str(struct in_str *i, const char *s)
{
	i->peek = static_peek;
	i->get = static_get;
#if ENABLE_HUSH_INTERACTIVE
	i->promptme = 1;
	i->promptmode = 0; /* PS1 */
#endif
	i->p = s;
	i->eof_flag = 0;
}

/* squirrel != NULL means we squirrel away copies of stdin, stdout,
 * and stderr if they are redirected. */
static int setup_redirects(struct child_prog *prog, int squirrel[])
{
	int openfd, mode;
	struct redir_struct *redir;

	for (redir = prog->redirects; redir; redir = redir->next) {
		if (redir->dup == -1 && redir->glob_word == NULL) {
			/* something went wrong in the parse.  Pretend it didn't happen */
			continue;
		}
		if (redir->dup == -1) {
			char *p;
			mode = redir_table[redir->type].mode;
			p = expand_string_to_string(redir->glob_word[0]);
			openfd = open_or_warn(p, mode);
			free(p);
			if (openfd < 0) {
			/* this could get lost if stderr has been redirected, but
			   bash and ash both lose it as well (though zsh doesn't!) */
				return 1;
			}
		} else {
			openfd = redir->dup;
		}

		if (openfd != redir->fd) {
			if (squirrel && redir->fd < 3) {
				squirrel[redir->fd] = dup(redir->fd);
			}
			if (openfd == -3) {
				//close(openfd); // close(-3) ??!
			} else {
				dup2(openfd, redir->fd);
				if (redir->dup == -1)
					close(openfd);
			}
		}
	}
	return 0;
}

static void restore_redirects(int squirrel[])
{
	int i, fd;
	for (i = 0; i < 3; i++) {
		fd = squirrel[i];
		if (fd != -1) {
			/* We simply die on error */
			xmove_fd(fd, i);
		}
	}
}

/* Called after [v]fork() in run_pipe(), or from builtin_exec().
 * Never returns.
 * XXX no exit() here.  If you don't exec, use _exit instead.
 * The at_exit handlers apparently confuse the calling process,
 * in particular stdin handling.  Not sure why? -- because of vfork! (vda) */
static void pseudo_exec_argv(char **argv)
{
	int i, rcode;
	char *p;
	const struct built_in_command *x;

	for (i = 0; is_assignment(argv[i]); i++) {
		debug_printf_exec("pid %d environment modification: %s\n",
				getpid(), argv[i]);
// FIXME: vfork case??
		p = expand_string_to_string(argv[i]);
		putenv(p);
	}
	argv += i;
	/* If a variable is assigned in a forest, and nobody listens,
	 * was it ever really set?
	 */
	if (!argv[0])
		_exit(EXIT_SUCCESS);

	argv = expand_strvec_to_strvec(argv);

	/*
	 * Check if the command matches any of the builtins.
	 * Depending on context, this might be redundant.  But it's
	 * easier to waste a few CPU cycles than it is to figure out
	 * if this is one of those cases.
	 */
	for (x = bltins; x->cmd; x++) {
		if (strcmp(argv[0], x->cmd) == 0) {
			debug_printf_exec("running builtin '%s'\n", argv[0]);
			rcode = x->function(argv);
			fflush(stdout);
			_exit(rcode);
		}
	}

	/* Check if the command matches any busybox applets */
#if ENABLE_FEATURE_SH_STANDALONE
	if (strchr(argv[0], '/') == NULL) {
		int a = find_applet_by_name(argv[0]);
		if (a >= 0) {
			if (APPLET_IS_NOEXEC(a)) {
				debug_printf_exec("running applet '%s'\n", argv[0]);
// is it ok that run_applet_no_and_exit() does exit(), not _exit()?
				run_applet_no_and_exit(a, argv);
			}
			/* re-exec ourselves with the new arguments */
			debug_printf_exec("re-execing applet '%s'\n", argv[0]);
			execvp(bb_busybox_exec_path, argv);
			/* If they called chroot or otherwise made the binary no longer
			 * executable, fall through */
		}
	}
#endif

	debug_printf_exec("execing '%s'\n", argv[0]);
	execvp(argv[0], argv);
	bb_perror_msg("cannot exec '%s'", argv[0]);
	_exit(1);
}

/* Called after [v]fork() in run_pipe()
 */
static void pseudo_exec(struct child_prog *child)
{
// FIXME: buggy wrt NOMMU! Must not modify any global data
// until it does exec/_exit, but currently it does
// (puts malloc'ed stuff into environment)
	if (child->argv)
		pseudo_exec_argv(child->argv);

	if (child->group) {
#if !BB_MMU
		bb_error_msg_and_die("nested lists are not supported on NOMMU");
#else
		int rcode;

#if ENABLE_HUSH_INTERACTIVE
// run_list_level now takes care of it?
//		debug_printf_exec("pseudo_exec: setting interactive_fd=0\n");
//		interactive_fd = 0;    /* crucial!!!! */
#endif
		debug_printf_exec("pseudo_exec: run_list\n");
		rcode = run_list(child->group);
		/* OK to leak memory by not calling free_pipe_list,
		 * since this process is about to exit */
		_exit(rcode);
#endif
	}

	/* Can happen.  See what bash does with ">foo" by itself. */
	debug_printf("trying to pseudo_exec null command\n");
	_exit(EXIT_SUCCESS);
}

#if ENABLE_HUSH_JOB
static const char *get_cmdtext(struct pipe *pi)
{
	char **argv;
	char *p;
	int len;

	/* This is subtle. ->cmdtext is created only on first backgrounding.
	 * (Think "cat, <ctrl-z>, fg, <ctrl-z>, fg, <ctrl-z>...." here...)
	 * On subsequent bg argv is trashed, but we won't use it */
	if (pi->cmdtext)
		return pi->cmdtext;
	argv = pi->progs[0].argv;
	if (!argv || !argv[0])
		return (pi->cmdtext = xzalloc(1));

	len = 0;
	do len += strlen(*argv) + 1; while (*++argv);
	pi->cmdtext = p = xmalloc(len);
	argv = pi->progs[0].argv;
	do {
		len = strlen(*argv);
		memcpy(p, *argv, len);
		p += len;
		*p++ = ' ';
	} while (*++argv);
	p[-1] = '\0';
	return pi->cmdtext;
}

static void insert_bg_job(struct pipe *pi)
{
	struct pipe *thejob;
	int i;

	/* Linear search for the ID of the job to use */
	pi->jobid = 1;
	for (thejob = job_list; thejob; thejob = thejob->next)
		if (thejob->jobid >= pi->jobid)
			pi->jobid = thejob->jobid + 1;

	/* Add thejob to the list of running jobs */
	if (!job_list) {
		thejob = job_list = xmalloc(sizeof(*thejob));
	} else {
		for (thejob = job_list; thejob->next; thejob = thejob->next)
			continue;
		thejob->next = xmalloc(sizeof(*thejob));
		thejob = thejob->next;
	}

	/* Physically copy the struct job */
	memcpy(thejob, pi, sizeof(struct pipe));
	thejob->progs = xzalloc(sizeof(pi->progs[0]) * pi->num_progs);
	/* We cannot copy entire pi->progs[] vector! Double free()s will happen */
	for (i = 0; i < pi->num_progs; i++) {
// TODO: do we really need to have so many fields which are just dead weight
// at execution stage?
		thejob->progs[i].pid = pi->progs[i].pid;
		/* all other fields are not used and stay zero */
	}
	thejob->next = NULL;
	thejob->cmdtext = xstrdup(get_cmdtext(pi));

	/* We don't wait for background thejobs to return -- append it
	   to the list of backgrounded thejobs and leave it alone */
	printf("[%d] %d %s\n", thejob->jobid, thejob->progs[0].pid, thejob->cmdtext);
	last_bg_pid = thejob->progs[0].pid;
	last_jobid = thejob->jobid;
}

static void remove_bg_job(struct pipe *pi)
{
	struct pipe *prev_pipe;

	if (pi == job_list) {
		job_list = pi->next;
	} else {
		prev_pipe = job_list;
		while (prev_pipe->next != pi)
			prev_pipe = prev_pipe->next;
		prev_pipe->next = pi->next;
	}
	if (job_list)
		last_jobid = job_list->jobid;
	else
		last_jobid = 0;
}

/* remove a backgrounded job */
static void delete_finished_bg_job(struct pipe *pi)
{
	remove_bg_job(pi);
	pi->stopped_progs = 0;
	free_pipe(pi, 0);
	free(pi);
}
#endif /* JOB */

/* Checks to see if any processes have exited -- if they
   have, figure out why and see if a job has completed */
static int checkjobs(struct pipe* fg_pipe)
{
	int attributes;
	int status;
#if ENABLE_HUSH_JOB
	int prognum = 0;
	struct pipe *pi;
#endif
	pid_t childpid;
	int rcode = 0;

	attributes = WUNTRACED;
	if (fg_pipe == NULL) {
		attributes |= WNOHANG;
	}

/* Do we do this right?
 * bash-3.00# sleep 20 | false
 * <ctrl-Z pressed>
 * [3]+  Stopped          sleep 20 | false
 * bash-3.00# echo $?
 * 1   <========== bg pipe is not fully done, but exitcode is already known!
 */

//FIXME: non-interactive bash does not continue even if all processes in fg pipe
//are stopped. Testcase: "cat | cat" in a script (not on command line)
// + killall -STOP cat

 wait_more:
// TODO: safe_waitpid?
	while ((childpid = waitpid(-1, &status, attributes)) > 0) {
		const int dead = WIFEXITED(status) || WIFSIGNALED(status);

#ifdef DEBUG_SHELL_JOBS
		if (WIFSTOPPED(status))
			debug_printf_jobs("pid %d stopped by sig %d (exitcode %d)\n",
					childpid, WSTOPSIG(status), WEXITSTATUS(status));
		if (WIFSIGNALED(status))
			debug_printf_jobs("pid %d killed by sig %d (exitcode %d)\n",
					childpid, WTERMSIG(status), WEXITSTATUS(status));
		if (WIFEXITED(status))
			debug_printf_jobs("pid %d exited, exitcode %d\n",
					childpid, WEXITSTATUS(status));
#endif
		/* Were we asked to wait for fg pipe? */
		if (fg_pipe) {
			int i;
			for (i = 0; i < fg_pipe->num_progs; i++) {
				debug_printf_jobs("check pid %d\n", fg_pipe->progs[i].pid);
				if (fg_pipe->progs[i].pid == childpid) {
					/* printf("process %d exit %d\n", i, WEXITSTATUS(status)); */
					if (dead) {
						fg_pipe->progs[i].pid = 0;
						fg_pipe->running_progs--;
						if (i == fg_pipe->num_progs - 1)
							/* last process gives overall exitstatus */
							rcode = WEXITSTATUS(status);
					} else {
						fg_pipe->progs[i].is_stopped = 1;
						fg_pipe->stopped_progs++;
					}
					debug_printf_jobs("fg_pipe: running_progs %d stopped_progs %d\n",
							fg_pipe->running_progs, fg_pipe->stopped_progs);
					if (fg_pipe->running_progs - fg_pipe->stopped_progs <= 0) {
						/* All processes in fg pipe have exited/stopped */
#if ENABLE_HUSH_JOB
						if (fg_pipe->running_progs)
							insert_bg_job(fg_pipe);
#endif
						return rcode;
					}
					/* There are still running processes in the fg pipe */
					goto wait_more;
				}
			}
			/* fall through to searching process in bg pipes */
		}

#if ENABLE_HUSH_JOB
		/* We asked to wait for bg or orphaned children */
		/* No need to remember exitcode in this case */
		for (pi = job_list; pi; pi = pi->next) {
			prognum = 0;
			while (prognum < pi->num_progs) {
				if (pi->progs[prognum].pid == childpid)
					goto found_pi_and_prognum;
				prognum++;
			}
		}
#endif

		/* Happens when shell is used as init process (init=/bin/sh) */
		debug_printf("checkjobs: pid %d was not in our list!\n", childpid);
		goto wait_more;

#if ENABLE_HUSH_JOB
 found_pi_and_prognum:
		if (dead) {
			/* child exited */
			pi->progs[prognum].pid = 0;
			pi->running_progs--;
			if (!pi->running_progs) {
				printf(JOB_STATUS_FORMAT, pi->jobid,
							"Done", pi->cmdtext);
				delete_finished_bg_job(pi);
			}
		} else {
			/* child stopped */
			pi->stopped_progs++;
			pi->progs[prognum].is_stopped = 1;
		}
#endif
	}

	/* wait found no children or failed */

	if (childpid && errno != ECHILD)
		bb_perror_msg("waitpid");
	return rcode;
}

#if ENABLE_HUSH_JOB
static int checkjobs_and_fg_shell(struct pipe* fg_pipe)
{
	pid_t p;
	int rcode = checkjobs(fg_pipe);
	/* Job finished, move the shell to the foreground */
	p = getpgid(0); /* pgid of our process */
	debug_printf_jobs("fg'ing ourself: getpgid(0)=%d\n", (int)p);
	if (tcsetpgrp(interactive_fd, p) && errno != ENOTTY)
		bb_perror_msg("tcsetpgrp-4a");
	return rcode;
}
#endif

/* run_pipe() starts all the jobs, but doesn't wait for anything
 * to finish.  See checkjobs().
 *
 * return code is normally -1, when the caller has to wait for children
 * to finish to determine the exit status of the pipe.  If the pipe
 * is a simple builtin command, however, the action is done by the
 * time run_pipe returns, and the exit code is provided as the
 * return value.
 *
 * The input of the pipe is always stdin, the output is always
 * stdout.  The outpipe[] mechanism in BusyBox-0.48 lash is bogus,
 * because it tries to avoid running the command substitution in
 * subshell, when that is in fact necessary.  The subshell process
 * now has its stdout directed to the input of the appropriate pipe,
 * so this routine is noticeably simpler.
 *
 * Returns -1 only if started some children. IOW: we have to
 * mask out retvals of builtins etc with 0xff!
 */
static int run_pipe(struct pipe *pi)
{
	int i;
	int nextin;
	int pipefds[2];		/* pipefds[0] is for reading */
	struct child_prog *child;
	const struct built_in_command *x;
	char *p;
	/* it is not always needed, but we aim to smaller code */
	int squirrel[] = { -1, -1, -1 };
	int rcode;
	const int single_fg = (pi->num_progs == 1 && pi->followup != PIPE_BG);

	debug_printf_exec("run_pipe start: single_fg=%d\n", single_fg);

#if ENABLE_HUSH_JOB
	pi->pgrp = -1;
#endif
	pi->running_progs = 1;
	pi->stopped_progs = 0;

	/* Check if this is a simple builtin (not part of a pipe).
	 * Builtins within pipes have to fork anyway, and are handled in
	 * pseudo_exec.  "echo foo | read bar" doesn't work on bash, either.
	 */
	child = &(pi->progs[0]);
	if (single_fg && child->group && child->subshell == 0) {
		debug_printf("non-subshell grouping\n");
		setup_redirects(child, squirrel);
		debug_printf_exec(": run_list\n");
		rcode = run_list(child->group) & 0xff;
		restore_redirects(squirrel);
		debug_printf_exec("run_pipe return %d\n", rcode);
		return rcode;
	}

	if (single_fg && child->argv != NULL) {
		char **argv_expanded;
		char **argv = child->argv;

		for (i = 0; is_assignment(argv[i]); i++)
			continue;
		if (i != 0 && argv[i] == NULL) {
			/* assignments, but no command: set the local environment */
			for (i = 0; argv[i] != NULL; i++) {
				debug_printf("local environment set: %s\n", argv[i]);
				p = expand_string_to_string(argv[i]);
				set_local_var(p, 0);
			}
			return EXIT_SUCCESS;   /* don't worry about errors in set_local_var() yet */
		}
		for (i = 0; is_assignment(argv[i]); i++) {
			p = expand_string_to_string(argv[i]);
			//sp: child->sp--;
			putenv(p);
		}
		for (x = bltins; x->cmd; x++) {
			if (strcmp(argv[i], x->cmd) == 0) {
				if (x->function == builtin_exec && argv[i+1] == NULL) {
					debug_printf("magic exec\n");
					setup_redirects(child, NULL);
					return EXIT_SUCCESS;
				}
				debug_printf("builtin inline %s\n", argv[0]);
				/* XXX setup_redirects acts on file descriptors, not FILEs.
				 * This is perfect for work that comes after exec().
				 * Is it really safe for inline use?  Experimentally,
				 * things seem to work with glibc. */
				setup_redirects(child, squirrel);
				debug_printf_exec(": builtin '%s' '%s'...\n", x->cmd, argv[i+1]);
				//sp: if (child->sp) /* btw we can do it unconditionally... */
				argv_expanded = expand_strvec_to_strvec(argv + i);
				rcode = x->function(argv_expanded) & 0xff;
				free(argv_expanded);
				restore_redirects(squirrel);
				debug_printf_exec("run_pipe return %d\n", rcode);
				return rcode;
			}
		}
#if ENABLE_FEATURE_SH_STANDALONE
		{
			int a = find_applet_by_name(argv[i]);
			if (a >= 0 && APPLET_IS_NOFORK(a)) {
				setup_redirects(child, squirrel);
				save_nofork_data(&nofork_save);
				argv_expanded = argv + i;
				//sp: if (child->sp)
				argv_expanded = expand_strvec_to_strvec(argv + i);
				debug_printf_exec(": run_nofork_applet '%s' '%s'...\n", argv_expanded[0], argv_expanded[1]);
				rcode = run_nofork_applet_prime(&nofork_save, a, argv_expanded) & 0xff;
				free(argv_expanded);
				restore_redirects(squirrel);
				debug_printf_exec("run_pipe return %d\n", rcode);
				return rcode;
			}
		}
#endif
	}

	/* Disable job control signals for shell (parent) and
	 * for initial child code after fork */
	set_jobctrl_sighandler(SIG_IGN);

	/* Going to fork a child per each pipe member */
	pi->running_progs = 0;
	nextin = 0;

	for (i = 0; i < pi->num_progs; i++) {
		child = &(pi->progs[i]);
		if (child->argv)
			debug_printf_exec(": pipe member '%s' '%s'...\n", child->argv[0], child->argv[1]);
		else
			debug_printf_exec(": pipe member with no argv\n");

		/* pipes are inserted between pairs of commands */
		pipefds[0] = 0;
		pipefds[1] = 1;
		if ((i + 1) < pi->num_progs)
			xpipe(pipefds);

		child->pid = BB_MMU ? fork() : vfork();
		if (!child->pid) { /* child */
#if ENABLE_HUSH_JOB
			/* Every child adds itself to new process group
			 * with pgid == pid_of_first_child_in_pipe */
			if (run_list_level == 1 && interactive_fd) {
				pid_t pgrp;
				/* Don't do pgrp restore anymore on fatal signals */
				set_fatal_sighandler(SIG_DFL);
				pgrp = pi->pgrp;
				if (pgrp < 0) /* true for 1st process only */
					pgrp = getpid();
				if (setpgid(0, pgrp) == 0 && pi->followup != PIPE_BG) {
					/* We do it in *every* child, not just first,
					 * to avoid races */
					tcsetpgrp(interactive_fd, pgrp);
				}
			}
#endif
			xmove_fd(nextin, 0);
			xmove_fd(pipefds[1], 1); /* write end */
			if (pipefds[0] > 1)
				close(pipefds[0]); /* read end */
			/* Like bash, explicit redirects override pipes,
			 * and the pipe fd is available for dup'ing. */
			setup_redirects(child, NULL);

			/* Restore default handlers just prior to exec */
			set_jobctrl_sighandler(SIG_DFL);
			set_misc_sighandler(SIG_DFL);
			signal(SIGCHLD, SIG_DFL);
			pseudo_exec(child); /* does not return */
		}

		if (child->pid < 0) { /* [v]fork failed */
			/* Clearly indicate, was it fork or vfork */
			bb_perror_msg(BB_MMU ? "fork" : "vfork");
		} else {
			pi->running_progs++;
#if ENABLE_HUSH_JOB
			/* Second and next children need to know pid of first one */
			if (pi->pgrp < 0)
				pi->pgrp = child->pid;
#endif
		}

		if (i)
			close(nextin);
		if ((i + 1) < pi->num_progs)
			close(pipefds[1]); /* write end */
		/* Pass read (output) pipe end to next iteration */
		nextin = pipefds[0];
	}

	if (!pi->running_progs) {
		debug_printf_exec("run_pipe return 1 (all forks failed, no children)\n");
		return 1;
	}

	debug_printf_exec("run_pipe return -1 (%u children started)\n", pi->running_progs);
	return -1;
}

#ifndef debug_print_tree
static void debug_print_tree(struct pipe *pi, int lvl)
{
	static const char *PIPE[] = {
		[PIPE_SEQ] = "SEQ",
		[PIPE_AND] = "AND",
		[PIPE_OR ] = "OR" ,
		[PIPE_BG ] = "BG" ,
	};
	static const char *RES[] = {
		[RES_NONE ] = "NONE" ,
#if ENABLE_HUSH_IF
		[RES_IF   ] = "IF"   ,
		[RES_THEN ] = "THEN" ,
		[RES_ELIF ] = "ELIF" ,
		[RES_ELSE ] = "ELSE" ,
		[RES_FI   ] = "FI"   ,
#endif
#if ENABLE_HUSH_LOOPS
		[RES_FOR  ] = "FOR"  ,
		[RES_WHILE] = "WHILE",
		[RES_UNTIL] = "UNTIL",
		[RES_DO   ] = "DO"   ,
		[RES_DONE ] = "DONE" ,
		[RES_IN   ] = "IN"   ,
#endif
		[RES_XXXX ] = "XXXX" ,
		[RES_SNTX ] = "SNTX" ,
	};

	int pin, prn;

	pin = 0;
	while (pi) {
		fprintf(stderr, "%*spipe %d res_word=%s followup=%d %s\n", lvl*2, "",
				pin, RES[pi->res_word], pi->followup, PIPE[pi->followup]);
		prn = 0;
		while (prn < pi->num_progs) {
			struct child_prog *child = &pi->progs[prn];
			char **argv = child->argv;

			fprintf(stderr, "%*s prog %d", lvl*2, "", prn);
			if (child->group) {
				fprintf(stderr, " group %s: (argv=%p)\n",
						(child->subshell ? "()" : "{}"),
						argv);
				debug_print_tree(child->group, lvl+1);
				prn++;
				continue;
			}
			if (argv) while (*argv) {
				fprintf(stderr, " '%s'", *argv);
				argv++;
			}
			fprintf(stderr, "\n");
			prn++;
		}
		pi = pi->next;
		pin++;
	}
}
#endif

/* NB: called by pseudo_exec, and therefore must not modify any
 * global data until exec/_exit (we can be a child after vfork!) */
static int run_list(struct pipe *pi)
{
	struct pipe *rpipe;
#if ENABLE_HUSH_LOOPS
	char *for_varname = NULL;
	char **for_lcur = NULL;
	char **for_list = NULL;
	int flag_rep = 0;
#endif
	int flag_skip = 1;
	int rcode = 0; /* probably for gcc only */
	int flag_restore = 0;
#if ENABLE_HUSH_IF
	int if_code = 0, next_if_code = 0;  /* need double-buffer to handle elif */
#else
	enum { if_code = 0, next_if_code = 0 };
#endif
	reserved_style rword;
	reserved_style skip_more_for_this_rword = RES_XXXX;

	debug_printf_exec("run_list start lvl %d\n", run_list_level + 1);

#if ENABLE_HUSH_LOOPS
	/* check syntax for "for" */
	for (rpipe = pi; rpipe; rpipe = rpipe->next) {
		if ((rpipe->res_word == RES_IN || rpipe->res_word == RES_FOR)
		 && (rpipe->next == NULL)
		) {
			syntax("malformed for"); /* no IN or no commands after IN */
			debug_printf_exec("run_list lvl %d return 1\n", run_list_level);
			return 1;
		}
		if ((rpipe->res_word == RES_IN && rpipe->next->res_word == RES_IN && rpipe->next->progs[0].argv != NULL)
		 || (rpipe->res_word == RES_FOR && rpipe->next->res_word != RES_IN)
		) {
			/* TODO: what is tested in the first condition? */
			syntax("malformed for"); /* 2nd condition: not followed by IN */
			debug_printf_exec("run_list lvl %d return 1\n", run_list_level);
			return 1;
		}
	}
#else
	rpipe = NULL;
#endif

#if ENABLE_HUSH_JOB
	/* Example of nested list: "while true; do { sleep 1 | exit 2; } done".
	 * We are saving state before entering outermost list ("while...done")
	 * so that ctrl-Z will correctly background _entire_ outermost list,
	 * not just a part of it (like "sleep 1 | exit 2") */
	if (++run_list_level == 1 && interactive_fd) {
		if (sigsetjmp(toplevel_jb, 1)) {
			/* ctrl-Z forked and we are parent; or ctrl-C.
			 * Sighandler has longjmped us here */
			signal(SIGINT, SIG_IGN);
			signal(SIGTSTP, SIG_IGN);
			/* Restore level (we can be coming from deep inside
			 * nested levels) */
			run_list_level = 1;
#if ENABLE_FEATURE_SH_STANDALONE
			if (nofork_save.saved) { /* if save area is valid */
				debug_printf_jobs("exiting nofork early\n");
				restore_nofork_data(&nofork_save);
			}
#endif
			if (ctrl_z_flag) {
				/* ctrl-Z has forked and stored pid of the child in pi->pid.
				 * Remember this child as background job */
				insert_bg_job(pi);
			} else {
				/* ctrl-C. We just stop doing whatever we were doing */
				bb_putchar('\n');
			}
			rcode = 0;
			goto ret;
		}
		/* ctrl-Z handler will store pid etc in pi */
		toplevel_list = pi;
		ctrl_z_flag = 0;
#if ENABLE_FEATURE_SH_STANDALONE
		nofork_save.saved = 0; /* in case we will run a nofork later */
#endif
		signal_SA_RESTART(SIGTSTP, handler_ctrl_z);
		signal(SIGINT, handler_ctrl_c);
	}
#endif /* JOB */

	for (; pi; pi = flag_restore ? rpipe : pi->next) {
//why?		int save_num_progs;
		rword = pi->res_word;
#if ENABLE_HUSH_LOOPS
		if (rword == RES_WHILE || rword == RES_UNTIL || rword == RES_FOR) {
			flag_restore = 0;
			if (!rpipe) {
				flag_rep = 0;
				rpipe = pi;
			}
		}
#endif
		debug_printf_exec(": rword=%d if_code=%d next_if_code=%d skip_more=%d\n",
				rword, if_code, next_if_code, skip_more_for_this_rword);
		if (rword == skip_more_for_this_rword && flag_skip) {
			if (pi->followup == PIPE_SEQ)
				flag_skip = 0;
			continue;
		}
		flag_skip = 1;
		skip_more_for_this_rword = RES_XXXX;
#if ENABLE_HUSH_IF
		if (rword == RES_THEN || rword == RES_ELSE)
			if_code = next_if_code;
		if (rword == RES_THEN && if_code)
			continue;
		if (rword == RES_ELSE && !if_code)
			continue;
		if (rword == RES_ELIF && !if_code)
			break;
#endif
#if ENABLE_HUSH_LOOPS
		if (rword == RES_FOR && pi->num_progs) {
			if (!for_lcur) {
				/* first loop through for */
				/* if no variable values after "in" we skip "for" */
				if (!pi->next->progs->argv)
					continue;
				/* create list of variable values */
				for_list = expand_strvec_to_strvec(pi->next->progs->argv);
				for_lcur = for_list;
				for_varname = pi->progs->argv[0];
				pi->progs->argv[0] = NULL;
				flag_rep = 1;
			}
			free(pi->progs->argv[0]);
			if (!*for_lcur) {
				/* for loop is over, clean up */
				free(for_list);
				for_lcur = NULL;
				flag_rep = 0;
				pi->progs->argv[0] = for_varname;
				continue;
			}
			/* insert next value from for_lcur */
			/* vda: does it need escaping? */
			pi->progs->argv[0] = xasprintf("%s=%s", for_varname, *for_lcur++);
		}
		if (rword == RES_IN)
			continue;
		if (rword == RES_DO) {
			if (!flag_rep)
				continue;
		}
		if (rword == RES_DONE) {
			if (flag_rep) {
				flag_restore = 1;
			} else {
				rpipe = NULL;
			}
		}
#endif
		if (pi->num_progs == 0)
			continue;
//why?		save_num_progs = pi->num_progs;
		debug_printf_exec(": run_pipe with %d members\n", pi->num_progs);
		rcode = run_pipe(pi);
		if (rcode != -1) {
			/* We only ran a builtin: rcode was set by the return value
			 * of run_pipe(), and we don't need to wait for anything. */
		} else if (pi->followup == PIPE_BG) {
			/* What does bash do with attempts to background builtins? */
			/* Even bash 3.2 doesn't do that well with nested bg:
			 * try "{ { sleep 10; echo DEEP; } & echo HERE; } &".
			 * I'm NOT treating inner &'s as jobs */
#if ENABLE_HUSH_JOB
			if (run_list_level == 1)
				insert_bg_job(pi);
#endif
			rcode = EXIT_SUCCESS;
		} else {
#if ENABLE_HUSH_JOB
			if (run_list_level == 1 && interactive_fd) {
				/* waits for completion, then fg's main shell */
				rcode = checkjobs_and_fg_shell(pi);
			} else
#endif
			{
				/* this one just waits for completion */
				rcode = checkjobs(pi);
			}
			debug_printf_exec(": checkjobs returned %d\n", rcode);
		}
		debug_printf_exec(": setting last_return_code=%d\n", rcode);
		last_return_code = rcode;
//why?		pi->num_progs = save_num_progs;
#if ENABLE_HUSH_IF
		if (rword == RES_IF || rword == RES_ELIF)
			next_if_code = rcode;  /* can be overwritten a number of times */
#endif
#if ENABLE_HUSH_LOOPS
		if (rword == RES_WHILE)
			flag_rep = !last_return_code;
		if (rword == RES_UNTIL)
			flag_rep = last_return_code;
#endif
		if ((rcode == EXIT_SUCCESS && pi->followup == PIPE_OR)
		 || (rcode != EXIT_SUCCESS && pi->followup == PIPE_AND)
		) {
			skip_more_for_this_rword = rword;
		}
		checkjobs(NULL);
	}

#if ENABLE_HUSH_JOB
	if (ctrl_z_flag) {
		/* ctrl-Z forked somewhere in the past, we are the child,
		 * and now we completed running the list. Exit. */
		exit(rcode);
	}
 ret:
	if (!--run_list_level && interactive_fd) {
		signal(SIGTSTP, SIG_IGN);
		signal(SIGINT, SIG_IGN);
	}
#endif
	debug_printf_exec("run_list lvl %d return %d\n", run_list_level + 1, rcode);
	return rcode;
}

/* return code is the exit status of the pipe */
static int free_pipe(struct pipe *pi, int indent)
{
	char **p;
	struct child_prog *child;
	struct redir_struct *r, *rnext;
	int a, i, ret_code = 0;

	if (pi->stopped_progs > 0)
		return ret_code;
	debug_printf_clean("%s run pipe: (pid %d)\n", indenter(indent), getpid());
	for (i = 0; i < pi->num_progs; i++) {
		child = &pi->progs[i];
		debug_printf_clean("%s  command %d:\n", indenter(indent), i);
		if (child->argv) {
			for (a = 0, p = child->argv; *p; a++, p++) {
				debug_printf_clean("%s   argv[%d] = %s\n", indenter(indent), a, *p);
			}
			free_strings(child->argv);
			child->argv = NULL;
		} else if (child->group) {
			debug_printf_clean("%s   begin group (subshell:%d)\n", indenter(indent), child->subshell);
			ret_code = free_pipe_list(child->group, indent+3);
			debug_printf_clean("%s   end group\n", indenter(indent));
		} else {
			debug_printf_clean("%s   (nil)\n", indenter(indent));
		}
		for (r = child->redirects; r; r = rnext) {
			debug_printf_clean("%s   redirect %d%s", indenter(indent), r->fd, redir_table[r->type].descrip);
			if (r->dup == -1) {
				/* guard against the case >$FOO, where foo is unset or blank */
				if (r->glob_word) {
					debug_printf_clean(" %s\n", r->glob_word[0]);
					free_strings(r->glob_word);
					r->glob_word = NULL;
				}
			} else {
				debug_printf_clean("&%d\n", r->dup);
			}
			rnext = r->next;
			free(r);
		}
		child->redirects = NULL;
	}
	free(pi->progs);   /* children are an array, they get freed all at once */
	pi->progs = NULL;
#if ENABLE_HUSH_JOB
	free(pi->cmdtext);
	pi->cmdtext = NULL;
#endif
	return ret_code;
}

static int free_pipe_list(struct pipe *head, int indent)
{
	int rcode = 0;   /* if list has no members */
	struct pipe *pi, *next;

	for (pi = head; pi; pi = next) {
		debug_printf_clean("%s pipe reserved mode %d\n", indenter(indent), pi->res_word);
		rcode = free_pipe(pi, indent);
		debug_printf_clean("%s pipe followup code %d\n", indenter(indent), pi->followup);
		next = pi->next;
		/*pi->next = NULL;*/
		free(pi);
	}
	return rcode;
}

/* Select which version we will use */
static int run_and_free_list(struct pipe *pi)
{
	int rcode = 0;
	debug_printf_exec("run_and_free_list entered\n");
	if (!fake_mode) {
		debug_printf_exec(": run_list with %d members\n", pi->num_progs);
		rcode = run_list(pi);
	}
	/* free_pipe_list has the side effect of clearing memory.
	 * In the long run that function can be merged with run_list,
	 * but doing that now would hobble the debugging effort. */
	free_pipe_list(pi, /* indent: */ 0);
	debug_printf_exec("run_nad_free_list return %d\n", rcode);
	return rcode;
}

/* Whoever decided to muck with glob internal data is AN IDIOT! */
/* uclibc happily changed the way it works (and it has rights to do so!),
   all hell broke loose (SEGVs) */

/* The API for glob is arguably broken.  This routine pushes a non-matching
 * string into the output structure, removing non-backslashed backslashes.
 * If someone can prove me wrong, by performing this function within the
 * original glob(3) api, feel free to rewrite this routine into oblivion.
 * XXX broken if the last character is '\\', check that before calling.
 */
static char **globhack(const char *src, char **strings)
{
	int cnt;
	const char *s;
	char *v, *dest;

	for (cnt = 1, s = src; s && *s; s++) {
		if (*s == '\\') s++;
		cnt++;
	}
	v = dest = xmalloc(cnt);
	for (s = src; s && *s; s++, dest++) {
		if (*s == '\\') s++;
		*dest = *s;
	}
	*dest = '\0';

	return add_string_to_strings(strings, v);
}

/* XXX broken if the last character is '\\', check that before calling */
static int glob_needed(const char *s)
{
	for (; *s; s++) {
		if (*s == '\\')
			s++;
		if (strchr("*[?", *s))
			return 1;
	}
	return 0;
}

static int xglob(o_string *dest, char ***pglob)
{
	/* short-circuit for null word */
	/* we can code this better when the debug_printf's are gone */
	if (dest->length == 0) {
		if (dest->nonnull) {
			/* bash man page calls this an "explicit" null */
			*pglob = globhack(dest->data, *pglob);
		}
		return 0;
	}

	if (glob_needed(dest->data)) {
		glob_t globdata;
		int gr;

		memset(&globdata, 0, sizeof(globdata));
		gr = glob(dest->data, 0, NULL, &globdata);
		debug_printf("glob returned %d\n", gr);
		if (gr == GLOB_NOSPACE)
			bb_error_msg_and_die("out of memory during glob");
		if (gr == GLOB_NOMATCH) {
			debug_printf("globhack returned %d\n", gr);
			/* quote removal, or more accurately, backslash removal */
			*pglob = globhack(dest->data, *pglob);
			globfree(&globdata);
			return 0;
		}
		if (gr != 0) { /* GLOB_ABORTED ? */
			bb_error_msg("glob(3) error %d", gr);
		}
		if (globdata.gl_pathv && globdata.gl_pathv[0])
			*pglob = add_strings_to_strings(1, *pglob, globdata.gl_pathv);
		globfree(&globdata);
		return gr;
	}

	*pglob = globhack(dest->data, *pglob);
	return 0;
}

/* expand_strvec_to_strvec() takes a list of strings, expands
 * all variable references within and returns a pointer to
 * a list of expanded strings, possibly with larger number
 * of strings. (Think VAR="a b"; echo $VAR).
 * This new list is allocated as a single malloc block.
 * NULL-terminated list of char* pointers is at the beginning of it,
 * followed by strings themself.
 * Caller can deallocate entire list by single free(list). */

/* Helpers first:
 * count_XXX estimates size of the block we need. It's okay
 * to over-estimate sizes a bit, if it makes code simpler */
static int count_ifs(const char *str)
{
	int cnt = 0;
	debug_printf_expand("count_ifs('%s') ifs='%s'", str, ifs);
	while (1) {
		str += strcspn(str, ifs);
		if (!*str) break;
		str++; /* str += strspn(str, ifs); */
		cnt++; /* cnt += strspn(str, ifs); - but this code is larger */
	}
	debug_printf_expand(" return %d\n", cnt);
	return cnt;
}

static void count_var_expansion_space(int *countp, int *lenp, char *arg)
{
	char first_ch;
	int i;
	int len = *lenp;
	int count = *countp;
	const char *val;
	char *p;

	while ((p = strchr(arg, SPECIAL_VAR_SYMBOL))) {
		len += p - arg;
		arg = ++p;
		p = strchr(p, SPECIAL_VAR_SYMBOL);
		first_ch = arg[0];

		switch (first_ch & 0x7f) {
		/* high bit in 1st_ch indicates that var is double-quoted */
		case '$': /* pid */
		case '!': /* bg pid */
		case '?': /* exitcode */
		case '#': /* argc */
			len += sizeof(int)*3 + 1; /* enough for int */
			break;
		case '*':
		case '@':
			for (i = 1; global_argv[i]; i++) {
				len += strlen(global_argv[i]) + 1;
				count++;
				if (!(first_ch & 0x80))
					count += count_ifs(global_argv[i]);
			}
			break;
		default:
			*p = '\0';
			arg[0] = first_ch & 0x7f;
			if (isdigit(arg[0])) {
				i = xatoi_u(arg);
				val = NULL;
				if (i < global_argc)
					val = global_argv[i];
			} else
				val = lookup_param(arg);
			arg[0] = first_ch;
			*p = SPECIAL_VAR_SYMBOL;

			if (val) {
				len += strlen(val) + 1;
				if (!(first_ch & 0x80))
					count += count_ifs(val);
			}
		}
		arg = ++p;
	}

	len += strlen(arg) + 1;
	count++;
	*lenp = len;
	*countp = count;
}

/* Store given string, finalizing the word and starting new one whenever
 * we encounter ifs char(s). This is used for expanding variable values.
 * End-of-string does NOT finalize word: think about 'echo -$VAR-' */
static int expand_on_ifs(char **list, int n, char **posp, const char *str)
{
	char *pos = *posp;
	while (1) {
		int word_len = strcspn(str, ifs);
		if (word_len) {
			memcpy(pos, str, word_len); /* store non-ifs chars */
			pos += word_len;
			str += word_len;
		}
		if (!*str)  /* EOL - do not finalize word */
			break;
		*pos++ = '\0';
		if (n) debug_printf_expand("expand_on_ifs finalized list[%d]=%p '%s' "
			"strlen=%d next=%p pos=%p\n", n-1, list[n-1], list[n-1],
			strlen(list[n-1]), list[n-1] + strlen(list[n-1]) + 1, pos);
		list[n++] = pos;
		str += strspn(str, ifs); /* skip ifs chars */
	}
	*posp = pos;
	return n;
}

/* Expand all variable references in given string, adding words to list[]
 * at n, n+1,... positions. Return updated n (so that list[n] is next one
 * to be filled). This routine is extremely tricky: has to deal with
 * variables/parameters with whitespace, $* and $@, and constructs like
 * 'echo -$*-'. If you play here, you must run testsuite afterwards! */
/* NB: another bug is that we cannot detect empty strings yet:
 * "" or $empty"" expands to zero words, has to expand to empty word */
static int expand_vars_to_list(char **list, int n, char **posp, char *arg, char or_mask)
{
	/* or_mask is either 0 (normal case) or 0x80
	 * (expansion of right-hand side of assignment == 1-element expand) */

	char first_ch, ored_ch;
	int i;
	const char *val;
	char *p;
	char *pos = *posp;

	ored_ch = 0;

	if (n) debug_printf_expand("expand_vars_to_list finalized list[%d]=%p '%s' "
		"strlen=%d next=%p pos=%p\n", n-1, list[n-1], list[n-1],
		strlen(list[n-1]), list[n-1] + strlen(list[n-1]) + 1, pos);
	list[n++] = pos;

	while ((p = strchr(arg, SPECIAL_VAR_SYMBOL))) {
		memcpy(pos, arg, p - arg);
		pos += (p - arg);
		arg = ++p;
		p = strchr(p, SPECIAL_VAR_SYMBOL);

		first_ch = arg[0] | or_mask; /* forced to "quoted" if or_mask = 0x80 */
		ored_ch |= first_ch;
		val = NULL;
		switch (first_ch & 0x7f) {
		/* Highest bit in first_ch indicates that var is double-quoted */
		case '$': /* pid */
			/* FIXME: (echo $$) should still print pid of main shell */
			val = utoa(getpid());
			break;
		case '!': /* bg pid */
			val = last_bg_pid ? utoa(last_bg_pid) : (char*)"";
			break;
		case '?': /* exitcode */
			val = utoa(last_return_code);
			break;
		case '#': /* argc */
			val = utoa(global_argc ? global_argc-1 : 0);
			break;
		case '*':
		case '@':
			i = 1;
			if (!global_argv[i])
				break;
			if (!(first_ch & 0x80)) { /* unquoted $* or $@ */
				while (global_argv[i]) {
					n = expand_on_ifs(list, n, &pos, global_argv[i]);
					debug_printf_expand("expand_vars_to_list: argv %d (last %d)\n", i, global_argc-1);
					if (global_argv[i++][0] && global_argv[i]) {
						/* this argv[] is not empty and not last:
						 * put terminating NUL, start new word */
						*pos++ = '\0';
						if (n) debug_printf_expand("expand_vars_to_list 2 finalized list[%d]=%p '%s' "
							"strlen=%d next=%p pos=%p\n", n-1, list[n-1], list[n-1],
							strlen(list[n-1]), list[n-1] + strlen(list[n-1]) + 1, pos);
						list[n++] = pos;
					}
				}
			} else
			/* If or_mask is nonzero, we handle assignment 'a=....$@.....'
			 * and in this case should treat it like '$*' - see 'else...' below */
			if (first_ch == ('@'|0x80) && !or_mask) { /* quoted $@ */
				while (1) {
					strcpy(pos, global_argv[i]);
					pos += strlen(global_argv[i]);
					if (++i >= global_argc)
						break;
					*pos++ = '\0';
					if (n) debug_printf_expand("expand_vars_to_list 3 finalized list[%d]=%p '%s' "
						"strlen=%d next=%p pos=%p\n", n-1, list[n-1], list[n-1],
							strlen(list[n-1]), list[n-1] + strlen(list[n-1]) + 1, pos);
					list[n++] = pos;
				}
			} else { /* quoted $*: add as one word */
				while (1) {
					strcpy(pos, global_argv[i]);
					pos += strlen(global_argv[i]);
					if (!global_argv[++i])
						break;
					if (ifs[0])
						*pos++ = ifs[0];
				}
			}
			break;
		default:
			*p = '\0';
			arg[0] = first_ch & 0x7f;
			if (isdigit(arg[0])) {
				i = xatoi_u(arg);
				val = NULL;
				if (i < global_argc)
					val = global_argv[i];
			} else
				val = lookup_param(arg);
			arg[0] = first_ch;
			*p = SPECIAL_VAR_SYMBOL;
			if (!(first_ch & 0x80)) { /* unquoted $VAR */
				if (val) {
					n = expand_on_ifs(list, n, &pos, val);
					val = NULL;
				}
			} /* else: quoted $VAR, val will be appended at pos */
		}
		if (val) {
			strcpy(pos, val);
			pos += strlen(val);
		}
		arg = ++p;
	}
	debug_printf_expand("expand_vars_to_list adding tail '%s' at %p\n", arg, pos);
	strcpy(pos, arg);
	pos += strlen(arg) + 1;
	if (pos == list[n-1] + 1) { /* expansion is empty */
		if (!(ored_ch & 0x80)) { /* all vars were not quoted... */
			debug_printf_expand("expand_vars_to_list list[%d] empty, going back\n", n);
			pos--;
			n--;
		}
	}

	*posp = pos;
	return n;
}

static char **expand_variables(char **argv, char or_mask)
{
	int n;
	int count = 1;
	int len = 0;
	char *pos, **v, **list;

	v = argv;
	if (!*v) debug_printf_expand("count_var_expansion_space: "
			"argv[0]=NULL count=%d len=%d alloc_space=%d\n",
			count, len, sizeof(char*) * count + len);
	while (*v) {
		count_var_expansion_space(&count, &len, *v);
		debug_printf_expand("count_var_expansion_space: "
			"'%s' count=%d len=%d alloc_space=%d\n",
			*v, count, len, sizeof(char*) * count + len);
		v++;
	}
	len += sizeof(char*) * count; /* total to alloc */
	list = xmalloc(len);
	pos = (char*)(list + count);
	debug_printf_expand("list=%p, list[0] should be %p\n", list, pos);
	n = 0;
	v = argv;
	while (*v)
		n = expand_vars_to_list(list, n, &pos, *v++, or_mask);

	if (n) debug_printf_expand("finalized list[%d]=%p '%s' "
		"strlen=%d next=%p pos=%p\n", n-1, list[n-1], list[n-1],
		strlen(list[n-1]), list[n-1] + strlen(list[n-1]) + 1, pos);
	list[n] = NULL;

#ifdef DEBUG_EXPAND
	{
		int m = 0;
		while (m <= n) {
			debug_printf_expand("list[%d]=%p '%s'\n", m, list[m], list[m]);
			m++;
		}
		debug_printf_expand("used_space=%d\n", pos - (char*)list);
	}
#endif
	if (ENABLE_HUSH_DEBUG)
		if (pos - (char*)list > len)
			bb_error_msg_and_die("BUG in varexp");
	return list;
}

static char **expand_strvec_to_strvec(char **argv)
{
	return expand_variables(argv, 0);
}

static char *expand_string_to_string(const char *str)
{
	char *argv[2], **list;

	argv[0] = (char*)str;
	argv[1] = NULL;
	list = expand_variables(argv, 0x80); /* 0x80: make one-element expansion */
	if (ENABLE_HUSH_DEBUG)
		if (!list[0] || list[1])
			bb_error_msg_and_die("BUG in varexp2");
	/* actually, just move string 2*sizeof(char*) bytes back */
	strcpy((char*)list, list[0]);
	debug_printf_expand("string_to_string='%s'\n", (char*)list);
	return (char*)list;
}

static char* expand_strvec_to_string(char **argv)
{
	char **list;

	list = expand_variables(argv, 0x80);
	/* Convert all NULs to spaces */
	if (list[0]) {
		int n = 1;
		while (list[n]) {
			if (ENABLE_HUSH_DEBUG)
				if (list[n-1] + strlen(list[n-1]) + 1 != list[n])
					bb_error_msg_and_die("BUG in varexp3");
			list[n][-1] = ' '; /* TODO: or to ifs[0]? */
			n++;
		}
	}
	strcpy((char*)list, list[0]);
	debug_printf_expand("strvec_to_string='%s'\n", (char*)list);
	return (char*)list;
}

/* This is used to get/check local shell variables */
static struct variable *get_local_var(const char *name)
{
	struct variable *cur;
	int len;

	if (!name)
		return NULL;
	len = strlen(name);
	for (cur = top_var; cur; cur = cur->next) {
		if (strncmp(cur->varstr, name, len) == 0 && cur->varstr[len] == '=')
			return cur;
	}
	return NULL;
}

/* str holds "NAME=VAL" and is expected to be malloced.
 * We take ownership of it. */
static int set_local_var(char *str, int flg_export)
{
	struct variable *cur;
	char *value;
	int name_len;

	value = strchr(str, '=');
	if (!value) { /* not expected to ever happen? */
		free(str);
		return -1;
	}

	name_len = value - str + 1; /* including '=' */
	cur = top_var; /* cannot be NULL (we have HUSH_VERSION and it's RO) */
	while (1) {
		if (strncmp(cur->varstr, str, name_len) != 0) {
			if (!cur->next) {
				/* Bail out. Note that now cur points
				 * to last var in linked list */
				break;
			}
			cur = cur->next;
			continue;
		}
		/* We found an existing var with this name */
		*value = '\0';
		if (cur->flg_read_only) {
			bb_error_msg("%s: readonly variable", str);
			free(str);
			return -1;
		}
		unsetenv(str); /* just in case */
		*value = '=';
		if (strcmp(cur->varstr, str) == 0) {
 free_and_exp:
			free(str);
			goto exp;
		}
		if (cur->max_len >= strlen(str)) {
			/* This one is from startup env, reuse space */
			strcpy(cur->varstr, str);
			goto free_and_exp;
		}
		/* max_len == 0 signifies "malloced" var, which we can
		 * (and has to) free */
		if (!cur->max_len)
			free(cur->varstr);
		cur->max_len = 0;
		goto set_str_and_exp;
	}

	/* Not found - create next variable struct */
	cur->next = xzalloc(sizeof(*cur));
	cur = cur->next;

 set_str_and_exp:
	cur->varstr = str;
 exp:
	if (flg_export)
		cur->flg_export = 1;
	if (cur->flg_export)
		return putenv(cur->varstr);
	return 0;
}

static void unset_local_var(const char *name)
{
	struct variable *cur;
	struct variable *prev = prev; /* for gcc */
	int name_len;

	if (!name)
		return;
	name_len = strlen(name);
	cur = top_var;
	while (cur) {
		if (strncmp(cur->varstr, name, name_len) == 0 && cur->varstr[name_len] == '=') {
			if (cur->flg_read_only) {
				bb_error_msg("%s: readonly variable", name);
				return;
			}
		/* prev is ok to use here because 1st variable, HUSH_VERSION,
		 * is ro, and we cannot reach this code on the 1st pass */
			prev->next = cur->next;
			unsetenv(cur->varstr);
			if (!cur->max_len)
				free(cur->varstr);
			free(cur);
			return;
		}
		prev = cur;
		cur = cur->next;
	}
}

static int is_assignment(const char *s)
{
	if (!s || !isalpha(*s))
		return 0;
	s++;
	while (isalnum(*s) || *s == '_')
		s++;
	return *s == '=';
}

/* the src parameter allows us to peek forward to a possible &n syntax
 * for file descriptor duplication, e.g., "2>&1".
 * Return code is 0 normally, 1 if a syntax error is detected in src.
 * Resource errors (in xmalloc) cause the process to exit */
static int setup_redirect(struct p_context *ctx, int fd, redir_type style,
	struct in_str *input)
{
	struct child_prog *child = ctx->child;
	struct redir_struct *redir = child->redirects;
	struct redir_struct *last_redir = NULL;

	/* Create a new redir_struct and drop it onto the end of the linked list */
	while (redir) {
		last_redir = redir;
		redir = redir->next;
	}
	redir = xzalloc(sizeof(struct redir_struct));
	/* redir->next = NULL; */
	/* redir->glob_word = NULL; */
	if (last_redir) {
		last_redir->next = redir;
	} else {
		child->redirects = redir;
	}

	redir->type = style;
	redir->fd = (fd == -1) ? redir_table[style].default_fd : fd;

	debug_printf("Redirect type %d%s\n", redir->fd, redir_table[style].descrip);

	/* Check for a '2>&1' type redirect */
	redir->dup = redirect_dup_num(input);
	if (redir->dup == -2) return 1;  /* syntax error */
	if (redir->dup != -1) {
		/* Erik had a check here that the file descriptor in question
		 * is legit; I postpone that to "run time"
		 * A "-" representation of "close me" shows up as a -3 here */
		debug_printf("Duplicating redirect '%d>&%d'\n", redir->fd, redir->dup);
	} else {
		/* We do _not_ try to open the file that src points to,
		 * since we need to return and let src be expanded first.
		 * Set ctx->pending_redirect, so we know what to do at the
		 * end of the next parsed word. */
		ctx->pending_redirect = redir;
	}
	return 0;
}

static struct pipe *new_pipe(void)
{
	struct pipe *pi;
	pi = xzalloc(sizeof(struct pipe));
	/*pi->num_progs = 0;*/
	/*pi->progs = NULL;*/
	/*pi->next = NULL;*/
	/*pi->followup = 0;  invalid */
	if (RES_NONE)
		pi->res_word = RES_NONE;
	return pi;
}

static void initialize_context(struct p_context *ctx)
{
	ctx->child = NULL;
	ctx->pipe = ctx->list_head = new_pipe();
	ctx->pending_redirect = NULL;
	ctx->res_w = RES_NONE;
	//only ctx->parse_type is not touched... is this intentional?
	ctx->old_flag = 0;
	ctx->stack = NULL;
	done_command(ctx);   /* creates the memory for working child */
}

/* normal return is 0
 * if a reserved word is found, and processed, return 1
 * should handle if, then, elif, else, fi, for, while, until, do, done.
 * case, function, and select are obnoxious, save those for later.
 */
#if ENABLE_HUSH_IF || ENABLE_HUSH_LOOPS
static int reserved_word(o_string *dest, struct p_context *ctx)
{
	struct reserved_combo {
		char literal[7];
		unsigned char code;
		int flag;
	};
	/* Mostly a list of accepted follow-up reserved words.
	 * FLAG_END means we are done with the sequence, and are ready
	 * to turn the compound list into a command.
	 * FLAG_START means the word must start a new compound list.
	 */
	static const struct reserved_combo reserved_list[] = {
#if ENABLE_HUSH_IF
		{ "if",    RES_IF,    FLAG_THEN | FLAG_START },
		{ "then",  RES_THEN,  FLAG_ELIF | FLAG_ELSE | FLAG_FI },
		{ "elif",  RES_ELIF,  FLAG_THEN },
		{ "else",  RES_ELSE,  FLAG_FI   },
		{ "fi",    RES_FI,    FLAG_END  },
#endif
#if ENABLE_HUSH_LOOPS
		{ "for",   RES_FOR,   FLAG_IN   | FLAG_START },
		{ "while", RES_WHILE, FLAG_DO   | FLAG_START },
		{ "until", RES_UNTIL, FLAG_DO   | FLAG_START },
		{ "in",    RES_IN,    FLAG_DO   },
		{ "do",    RES_DO,    FLAG_DONE },
		{ "done",  RES_DONE,  FLAG_END  }
#endif
	};

	const struct reserved_combo *r;

	for (r = reserved_list;	r < reserved_list + ARRAY_SIZE(reserved_list); r++) {
		if (strcmp(dest->data, r->literal) != 0)
			continue;
		debug_printf("found reserved word %s, code %d\n", r->literal, r->code);
		if (r->flag & FLAG_START) {
			struct p_context *new;
			debug_printf("push stack\n");
#if ENABLE_HUSH_LOOPS
			if (ctx->res_w == RES_IN || ctx->res_w == RES_FOR) {
				syntax("malformed for"); /* example: 'for if' */
				ctx->res_w = RES_SNTX;
				b_reset(dest);
				return 1;
			}
#endif
			new = xmalloc(sizeof(*new));
			*new = *ctx;   /* physical copy */
			initialize_context(ctx);
			ctx->stack = new;
		} else if (ctx->res_w == RES_NONE || !(ctx->old_flag & (1 << r->code))) {
			syntax(NULL);
			ctx->res_w = RES_SNTX;
			b_reset(dest);
			return 1;
		}
		ctx->res_w = r->code;
		ctx->old_flag = r->flag;
		if (ctx->old_flag & FLAG_END) {
			struct p_context *old;
			debug_printf("pop stack\n");
			done_pipe(ctx, PIPE_SEQ);
			old = ctx->stack;
			old->child->group = ctx->list_head;
			old->child->subshell = 0;
			*ctx = *old;   /* physical copy */
			free(old);
		}
		b_reset(dest);
		return 1;
	}
	return 0;
}
#else
#define reserved_word(dest, ctx) ((int)0)
#endif

/* Normal return is 0.
 * Syntax or xglob errors return 1. */
static int done_word(o_string *dest, struct p_context *ctx)
{
	struct child_prog *child = ctx->child;
	char ***glob_target;
	int gr;

	debug_printf_parse("done_word entered: '%s' %p\n", dest->data, child);
	if (dest->length == 0 && !dest->nonnull) {
		debug_printf_parse("done_word return 0: true null, ignored\n");
		return 0;
	}
	if (ctx->pending_redirect) {
		glob_target = &ctx->pending_redirect->glob_word;
	} else {
		if (child->group) {
			syntax(NULL);
			debug_printf_parse("done_word return 1: syntax error, groups and arglists don't mix\n");
			return 1;
		}
		if (!child->argv && (ctx->parse_type & PARSEFLAG_SEMICOLON)) {
			debug_printf_parse(": checking '%s' for reserved-ness\n", dest->data);
			if (reserved_word(dest, ctx)) {
				debug_printf_parse("done_word return %d\n", (ctx->res_w == RES_SNTX));
				return (ctx->res_w == RES_SNTX);
			}
		}
		glob_target = &child->argv;
	}
	gr = xglob(dest, glob_target);
	if (gr != 0) {
		debug_printf_parse("done_word return 1: xglob returned %d\n", gr);
		return 1;
	}

	b_reset(dest);
	if (ctx->pending_redirect) {
		/* NB: don't free_strings(ctx->pending_redirect->glob_word) here */
		if (ctx->pending_redirect->glob_word
		 && ctx->pending_redirect->glob_word[0]
		 && ctx->pending_redirect->glob_word[1]
		) {
			/* more than one word resulted from globbing redir */
			ctx->pending_redirect = NULL;
			bb_error_msg("ambiguous redirect");
			debug_printf_parse("done_word return 1: ambiguous redirect\n");
			return 1;
		}
		ctx->pending_redirect = NULL;
	}
#if ENABLE_HUSH_LOOPS
	if (ctx->res_w == RES_FOR) {
		done_word(dest, ctx);
		done_pipe(ctx, PIPE_SEQ);
	}
#endif
	debug_printf_parse("done_word return 0\n");
	return 0;
}

/* The only possible error here is out of memory, in which case
 * xmalloc exits. */
static int done_command(struct p_context *ctx)
{
	/* The child is really already in the pipe structure, so
	 * advance the pipe counter and make a new, null child. */
	struct pipe *pi = ctx->pipe;
	struct child_prog *child = ctx->child;

	if (child) {
		if (child->group == NULL
		 && child->argv == NULL
		 && child->redirects == NULL
		) {
			debug_printf_parse("done_command: skipping null cmd, num_progs=%d\n", pi->num_progs);
			return pi->num_progs;
		}
		pi->num_progs++;
		debug_printf_parse("done_command: ++num_progs=%d\n", pi->num_progs);
	} else {
		debug_printf_parse("done_command: initializing, num_progs=%d\n", pi->num_progs);
	}

	/* Only real trickiness here is that the uncommitted
	 * child structure is not counted in pi->num_progs. */
	pi->progs = xrealloc(pi->progs, sizeof(*pi->progs) * (pi->num_progs+1));
	child = &pi->progs[pi->num_progs];

	memset(child, 0, sizeof(*child));
	/*child->redirects = NULL;*/
	/*child->argv = NULL;*/
	/*child->is_stopped = 0;*/
	/*child->group = NULL;*/
	child->family = pi;
	//sp: /*child->sp = 0;*/
	//pt: child->parse_type = ctx->parse_type;

	ctx->child = child;
	/* but ctx->pipe and ctx->list_head remain unchanged */

	return pi->num_progs; /* used only for 0/nonzero check */
}

static int done_pipe(struct p_context *ctx, pipe_style type)
{
	struct pipe *new_p;
	int not_null;

	debug_printf_parse("done_pipe entered, followup %d\n", type);
	not_null = done_command(ctx);  /* implicit closure of previous command */
	ctx->pipe->followup = type;
	ctx->pipe->res_word = ctx->res_w;
	/* Without this check, even just <enter> on command line generates
	 * tree of three NOPs (!). Which is harmless but annoying.
	 * IOW: it is safe to do it unconditionally. */
	if (not_null) {
		new_p = new_pipe();
		ctx->pipe->next = new_p;
		ctx->pipe = new_p;
		ctx->child = NULL;
		done_command(ctx);  /* set up new pipe to accept commands */
	}
	debug_printf_parse("done_pipe return 0\n");
	return 0;
}

/* peek ahead in the in_str to find out if we have a "&n" construct,
 * as in "2>&1", that represents duplicating a file descriptor.
 * returns either -2 (syntax error), -1 (no &), or the number found.
 */
static int redirect_dup_num(struct in_str *input)
{
	int ch, d = 0, ok = 0;
	ch = b_peek(input);
	if (ch != '&') return -1;

	b_getch(input);  /* get the & */
	ch = b_peek(input);
	if (ch == '-') {
		b_getch(input);
		return -3;  /* "-" represents "close me" */
	}
	while (isdigit(ch)) {
		d = d*10 + (ch-'0');
		ok = 1;
		b_getch(input);
		ch = b_peek(input);
	}
	if (ok) return d;

	bb_error_msg("ambiguous redirect");
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

	if (o->length == 0)
		return -1;
	for (num = 0; num < o->length; num++) {
		if (!isdigit(*(o->data + num))) {
			return -1;
		}
	}
	/* reuse num (and save an int) */
	num = atoi(o->data);
	b_reset(o);
	return num;
}

#if ENABLE_HUSH_TICK
/* NB: currently disabled on NOMMU */
static FILE *generate_stream_from_list(struct pipe *head)
{
	FILE *pf;
	int pid, channel[2];

	xpipe(channel);
/* *** NOMMU WARNING *** */
/* By using vfork here, we suspend parent till child exits or execs.
 * If child will not do it before it fills the pipe, it can block forever
 * in write(STDOUT_FILENO), and parent (shell) will be also stuck.
 */
	pid = BB_MMU ? fork() : vfork();
	if (pid < 0)
		bb_perror_msg_and_die(BB_MMU ? "fork" : "vfork");
	if (pid == 0) { /* child */
		close(channel[0]);
		xmove_fd(channel[1], 1);
		/* Prevent it from trying to handle ctrl-z etc */
#if ENABLE_HUSH_JOB
		run_list_level = 1;
#endif
		/* Process substitution is not considered to be usual
		 * 'command execution'.
		 * SUSv3 says ctrl-Z should be ignored, ctrl-C should not. */
		/* Not needed, we are relying on it being disabled
		 * everywhere outside actual command execution. */
		/*set_jobctrl_sighandler(SIG_IGN);*/
		set_misc_sighandler(SIG_DFL);
		_exit(run_list(head));   /* leaks memory */
	}
	close(channel[1]);
	pf = fdopen(channel[0], "r");
	return pf;
	/* head is freed by the caller */
}

/* Return code is exit status of the process that is run. */
static int process_command_subs(o_string *dest, struct p_context *ctx,
	struct in_str *input, const char *subst_end)
{
	int retcode, ch, eol_cnt;
	o_string result = NULL_O_STRING;
	struct p_context inner;
	FILE *p;
	struct in_str pipe_str;

	initialize_context(&inner);

	/* recursion to generate command */
	retcode = parse_stream(&result, &inner, input, subst_end);
	if (retcode != 0)
		return retcode;  /* syntax error or EOF */
	done_word(&result, &inner);
	done_pipe(&inner, PIPE_SEQ);
	b_free(&result);

	p = generate_stream_from_list(inner.list_head);
	if (p == NULL)
		return 1;
	close_on_exec_on(fileno(p));
	setup_file_in_str(&pipe_str, p);

	/* now send results of command back into original context */
	eol_cnt = 0;
	while ((ch = b_getch(&pipe_str)) != EOF) {
		if (ch == '\n') {
			eol_cnt++;
			continue;
		}
		while (eol_cnt) {
			b_addqchr(dest, '\n', dest->o_quote);
			eol_cnt--;
		}
		b_addqchr(dest, ch, dest->o_quote);
	}

	debug_printf("done reading from pipe, pclose()ing\n");
	/* This is the step that wait()s for the child.  Should be pretty
	 * safe, since we just read an EOF from its stdout.  We could try
	 * to do better, by using wait(), and keeping track of background jobs
	 * at the same time.  That would be a lot of work, and contrary
	 * to the KISS philosophy of this program. */
	retcode = fclose(p);
	free_pipe_list(inner.list_head, /* indent: */ 0);
	debug_printf("closed FILE from child, retcode=%d\n", retcode);
	return retcode;
}
#endif

static int parse_group(o_string *dest, struct p_context *ctx,
	struct in_str *input, int ch)
{
	int rcode;
	const char *endch = NULL;
	struct p_context sub;
	struct child_prog *child = ctx->child;

	debug_printf_parse("parse_group entered\n");
	if (child->argv) {
		syntax(NULL);
		debug_printf_parse("parse_group return 1: syntax error, groups and arglists don't mix\n");
		return 1;
	}
	initialize_context(&sub);
	endch = "}";
	if (ch == '(') {
		endch = ")";
		child->subshell = 1;
	}
	rcode = parse_stream(dest, &sub, input, endch);
//vda: err chk?
	done_word(dest, &sub); /* finish off the final word in the subcontext */
	done_pipe(&sub, PIPE_SEQ);  /* and the final command there, too */
	child->group = sub.list_head;

	debug_printf_parse("parse_group return %d\n", rcode);
	return rcode;
	/* child remains "open", available for possible redirects */
}

/* Basically useful version until someone wants to get fancier,
 * see the bash man page under "Parameter Expansion" */
static const char *lookup_param(const char *src)
{
	struct variable *var = get_local_var(src);
	if (var)
		return strchr(var->varstr, '=') + 1;
	return NULL;
}

/* return code: 0 for OK, 1 for syntax error */
static int handle_dollar(o_string *dest, struct p_context *ctx, struct in_str *input)
{
	int ch = b_peek(input);  /* first character after the $ */
	unsigned char quote_mask = dest->o_quote ? 0x80 : 0;

	debug_printf_parse("handle_dollar entered: ch='%c'\n", ch);
	if (isalpha(ch)) {
		b_addchr(dest, SPECIAL_VAR_SYMBOL);
		//sp: ctx->child->sp++;
		while (1) {
			debug_printf_parse(": '%c'\n", ch);
			b_getch(input);
			b_addchr(dest, ch | quote_mask);
			quote_mask = 0;
			ch = b_peek(input);
			if (!isalnum(ch) && ch != '_')
				break;
		}
		b_addchr(dest, SPECIAL_VAR_SYMBOL);
	} else if (isdigit(ch)) {
 make_one_char_var:
		b_addchr(dest, SPECIAL_VAR_SYMBOL);
		//sp: ctx->child->sp++;
		debug_printf_parse(": '%c'\n", ch);
		b_getch(input);
		b_addchr(dest, ch | quote_mask);
		b_addchr(dest, SPECIAL_VAR_SYMBOL);
	} else switch (ch) {
		case '$': /* pid */
		case '!': /* last bg pid */
		case '?': /* last exit code */
		case '#': /* number of args */
		case '*': /* args */
		case '@': /* args */
			goto make_one_char_var;
		case '{':
			b_addchr(dest, SPECIAL_VAR_SYMBOL);
			//sp: ctx->child->sp++;
			b_getch(input);
			/* XXX maybe someone will try to escape the '}' */
			while (1) {
				ch = b_getch(input);
				if (ch == '}')
					break;
				if (!isalnum(ch) && ch != '_') {
					syntax("unterminated ${name}");
					debug_printf_parse("handle_dollar return 1: unterminated ${name}\n");
					return 1;
				}
				debug_printf_parse(": '%c'\n", ch);
				b_addchr(dest, ch | quote_mask);
				quote_mask = 0;
			}
			b_addchr(dest, SPECIAL_VAR_SYMBOL);
			break;
#if ENABLE_HUSH_TICK
		case '(':
			b_getch(input);
			process_command_subs(dest, ctx, input, ")");
			break;
#endif
		case '-':
		case '_':
			/* still unhandled, but should be eventually */
			bb_error_msg("unhandled syntax: $%c", ch);
			return 1;
			break;
		default:
			b_addqchr(dest, '$', dest->o_quote);
	}
	debug_printf_parse("handle_dollar return 0\n");
	return 0;
}

/* return code is 0 for normal exit, 1 for syntax error */
static int parse_stream(o_string *dest, struct p_context *ctx,
	struct in_str *input, const char *end_trigger)
{
	int ch, m;
	int redir_fd;
	redir_type redir_style;
	int next;

	/* Only double-quote state is handled in the state variable dest->o_quote.
	 * A single-quote triggers a bypass of the main loop until its mate is
	 * found.  When recursing, quote state is passed in via dest->o_quote. */

	debug_printf_parse("parse_stream entered, end_trigger='%s'\n", end_trigger);

	while (1) {
		m = CHAR_IFS;
		next = '\0';
		ch = b_getch(input);
		if (ch != EOF) {
			m = charmap[ch];
			if (ch != '\n')
				next = b_peek(input);
		}
		debug_printf_parse(": ch=%c (%d) m=%d quote=%d\n",
						ch, ch, m, dest->o_quote);
		if (m == CHAR_ORDINARY
		 || (m != CHAR_SPECIAL && dest->o_quote)
		) {
			if (ch == EOF) {
				syntax("unterminated \"");
				debug_printf_parse("parse_stream return 1: unterminated \"\n");
				return 1;
			}
			b_addqchr(dest, ch, dest->o_quote);
			continue;
		}
		if (m == CHAR_IFS) {
			if (done_word(dest, ctx)) {
				debug_printf_parse("parse_stream return 1: done_word!=0\n");
				return 1;
			}
			if (ch == EOF)
				break;
			/* If we aren't performing a substitution, treat
			 * a newline as a command separator.
			 * [why we don't handle it exactly like ';'? --vda] */
			if (end_trigger && ch == '\n') {
				done_pipe(ctx, PIPE_SEQ);
			}
		}
		if ((end_trigger && strchr(end_trigger, ch))
		 && !dest->o_quote && ctx->res_w == RES_NONE
		) {
			debug_printf_parse("parse_stream return 0: end_trigger char found\n");
			return 0;
		}
		if (m == CHAR_IFS)
			continue;
		switch (ch) {
		case '#':
			if (dest->length == 0 && !dest->o_quote) {
				while (1) {
					ch = b_peek(input);
					if (ch == EOF || ch == '\n')
						break;
					b_getch(input);
				}
			} else {
				b_addqchr(dest, ch, dest->o_quote);
			}
			break;
		case '\\':
			if (next == EOF) {
				syntax("\\<eof>");
				debug_printf_parse("parse_stream return 1: \\<eof>\n");
				return 1;
			}
			b_addqchr(dest, '\\', dest->o_quote);
			b_addqchr(dest, b_getch(input), dest->o_quote);
			break;
		case '$':
			if (handle_dollar(dest, ctx, input) != 0) {
				debug_printf_parse("parse_stream return 1: handle_dollar returned non-0\n");
				return 1;
			}
			break;
		case '\'':
			dest->nonnull = 1;
			while (1) {
				ch = b_getch(input);
				if (ch == EOF || ch == '\'')
					break;
				b_addchr(dest, ch);
			}
			if (ch == EOF) {
				syntax("unterminated '");
				debug_printf_parse("parse_stream return 1: unterminated '\n");
				return 1;
			}
			break;
		case '"':
			dest->nonnull = 1;
			dest->o_quote ^= 1; /* invert */
			break;
#if ENABLE_HUSH_TICK
		case '`':
			process_command_subs(dest, ctx, input, "`");
			break;
#endif
		case '>':
			redir_fd = redirect_opt_num(dest);
			done_word(dest, ctx);
			redir_style = REDIRECT_OVERWRITE;
			if (next == '>') {
				redir_style = REDIRECT_APPEND;
				b_getch(input);
			}
#if 0
			else if (next == '(') {
				syntax(">(process) not supported");
				debug_printf_parse("parse_stream return 1: >(process) not supported\n");
				return 1;
			}
#endif
			setup_redirect(ctx, redir_fd, redir_style, input);
			break;
		case '<':
			redir_fd = redirect_opt_num(dest);
			done_word(dest, ctx);
			redir_style = REDIRECT_INPUT;
			if (next == '<') {
				redir_style = REDIRECT_HEREIS;
				b_getch(input);
			} else if (next == '>') {
				redir_style = REDIRECT_IO;
				b_getch(input);
			}
#if 0
			else if (next == '(') {
				syntax("<(process) not supported");
				debug_printf_parse("parse_stream return 1: <(process) not supported\n");
				return 1;
			}
#endif
			setup_redirect(ctx, redir_fd, redir_style, input);
			break;
		case ';':
			done_word(dest, ctx);
			done_pipe(ctx, PIPE_SEQ);
			break;
		case '&':
			done_word(dest, ctx);
			if (next == '&') {
				b_getch(input);
				done_pipe(ctx, PIPE_AND);
			} else {
				done_pipe(ctx, PIPE_BG);
			}
			break;
		case '|':
			done_word(dest, ctx);
			if (next == '|') {
				b_getch(input);
				done_pipe(ctx, PIPE_OR);
			} else {
				/* we could pick up a file descriptor choice here
				 * with redirect_opt_num(), but bash doesn't do it.
				 * "echo foo 2| cat" yields "foo 2". */
				done_command(ctx);
			}
			break;
		case '(':
		case '{':
			if (parse_group(dest, ctx, input, ch) != 0) {
				debug_printf_parse("parse_stream return 1: parse_group returned non-0\n");
				return 1;
			}
			break;
		case ')':
		case '}':
			syntax("unexpected }");   /* Proper use of this character is caught by end_trigger */
			debug_printf_parse("parse_stream return 1: unexpected '}'\n");
			return 1;
		default:
			if (ENABLE_HUSH_DEBUG)
				bb_error_msg_and_die("BUG: unexpected %c\n", ch);
		}
	}
	/* Complain if quote?  No, maybe we just finished a command substitution
	 * that was quoted.  Example:
	 * $ echo "`cat foo` plus more"
	 * and we just got the EOF generated by the subshell that ran "cat foo"
	 * The only real complaint is if we got an EOF when end_trigger != NULL,
	 * that is, we were really supposed to get end_trigger, and never got
	 * one before the EOF.  Can't use the standard "syntax error" return code,
	 * so that parse_stream_outer can distinguish the EOF and exit smoothly. */
	debug_printf_parse("parse_stream return %d\n", -(end_trigger != NULL));
	if (end_trigger)
		return -1;
	return 0;
}

static void set_in_charmap(const char *set, int code)
{
	while (*set)
		charmap[(unsigned char)*set++] = code;
}

static void update_charmap(void)
{
	/* char *ifs and char charmap[256] are both globals. */
	ifs = getenv("IFS");
	if (ifs == NULL)
		ifs = " \t\n";
	/* Precompute a list of 'flow through' behavior so it can be treated
	 * quickly up front.  Computation is necessary because of IFS.
	 * Special case handling of IFS == " \t\n" is not implemented.
	 * The charmap[] array only really needs two bits each,
	 * and on most machines that would be faster (reduced L1 cache use).
	 */
	memset(charmap, CHAR_ORDINARY, sizeof(charmap));
#if ENABLE_HUSH_TICK
	set_in_charmap("\\$\"`", CHAR_SPECIAL);
#else
	set_in_charmap("\\$\"", CHAR_SPECIAL);
#endif
	set_in_charmap("<>;&|(){}#'", CHAR_ORDINARY_IF_QUOTED);
	set_in_charmap(ifs, CHAR_IFS);  /* are ordinary if quoted */
}

/* most recursion does not come through here, the exception is
 * from builtin_source() and builtin_eval() */
static int parse_and_run_stream(struct in_str *inp, int parse_flag)
{
	struct p_context ctx;
	o_string temp = NULL_O_STRING;
	int rcode;
	do {
		ctx.parse_type = parse_flag;
		initialize_context(&ctx);
		update_charmap();
		if (!(parse_flag & PARSEFLAG_SEMICOLON) || (parse_flag & PARSEFLAG_REPARSING))
			set_in_charmap(";$&|", CHAR_ORDINARY);
#if ENABLE_HUSH_INTERACTIVE
		inp->promptmode = 0; /* PS1 */
#endif
		/* We will stop & execute after each ';' or '\n'.
		 * Example: "sleep 9999; echo TEST" + ctrl-C:
		 * TEST should be printed */
		rcode = parse_stream(&temp, &ctx, inp, ";\n");
		if (rcode != 1 && ctx.old_flag != 0) {
			syntax(NULL);
		}
		if (rcode != 1 && ctx.old_flag == 0) {
			done_word(&temp, &ctx);
			done_pipe(&ctx, PIPE_SEQ);
			debug_print_tree(ctx.list_head, 0);
			debug_printf_exec("parse_stream_outer: run_and_free_list\n");
			run_and_free_list(ctx.list_head);
		} else {
			if (ctx.old_flag != 0) {
				free(ctx.stack);
				b_reset(&temp);
			}
			temp.nonnull = 0;
			temp.o_quote = 0;
			inp->p = NULL;
			free_pipe_list(ctx.list_head, /* indent: */ 0);
		}
		b_free(&temp);
	} while (rcode != -1 && !(parse_flag & PARSEFLAG_EXIT_FROM_LOOP));   /* loop on syntax errors, return on EOF */
	return 0;
}

static int parse_and_run_string(const char *s, int parse_flag)
{
	struct in_str input;
	setup_string_in_str(&input, s);
	return parse_and_run_stream(&input, parse_flag);
}

static int parse_and_run_file(FILE *f)
{
	int rcode;
	struct in_str input;
	setup_file_in_str(&input, f);
	rcode = parse_and_run_stream(&input, PARSEFLAG_SEMICOLON);
	return rcode;
}

#if ENABLE_HUSH_JOB
/* Make sure we have a controlling tty.  If we get started under a job
 * aware app (like bash for example), make sure we are now in charge so
 * we don't fight over who gets the foreground */
static void setup_job_control(void)
{
	pid_t shell_pgrp;

	saved_task_pgrp = shell_pgrp = getpgrp();
	debug_printf_jobs("saved_task_pgrp=%d\n", saved_task_pgrp);
	close_on_exec_on(interactive_fd);

	/* If we were ran as 'hush &',
	 * sleep until we are in the foreground.  */
	while (tcgetpgrp(interactive_fd) != shell_pgrp) {
		/* Send TTIN to ourself (should stop us) */
		kill(- shell_pgrp, SIGTTIN);
		shell_pgrp = getpgrp();
	}

	/* Ignore job-control and misc signals.  */
	set_jobctrl_sighandler(SIG_IGN);
	set_misc_sighandler(SIG_IGN);
//huh?	signal(SIGCHLD, SIG_IGN);

	/* We _must_ restore tty pgrp on fatal signals */
	set_fatal_sighandler(sigexit);

	/* Put ourselves in our own process group.  */
	setpgrp(); /* is the same as setpgid(our_pid, our_pid); */
	/* Grab control of the terminal.  */
	tcsetpgrp(interactive_fd, getpid());
}
#endif

int hush_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int hush_main(int argc, char **argv)
{
	static const char version_str[] ALIGN1 = "HUSH_VERSION="HUSH_VER_STR;
	static const struct variable const_shell_ver = {
		.next = NULL,
		.varstr = (char*)version_str,
		.max_len = 1, /* 0 can provoke free(name) */
		.flg_export = 1,
		.flg_read_only = 1,
	};

	int opt;
	FILE *input;
	char **e;
	struct variable *cur_var;

	PTR_TO_GLOBALS = xzalloc(sizeof(G));

	/* Deal with HUSH_VERSION */
	shell_ver = const_shell_ver; /* copying struct here */
	top_var = &shell_ver;
	unsetenv("HUSH_VERSION"); /* in case it exists in initial env */
	/* Initialize our shell local variables with the values
	 * currently living in the environment */
	cur_var = top_var;
	e = environ;
	if (e) while (*e) {
		char *value = strchr(*e, '=');
		if (value) { /* paranoia */
			cur_var->next = xzalloc(sizeof(*cur_var));
			cur_var = cur_var->next;
			cur_var->varstr = *e;
			cur_var->max_len = strlen(*e);
			cur_var->flg_export = 1;
		}
		e++;
	}
	putenv((char *)version_str); /* reinstate HUSH_VERSION */

#if ENABLE_FEATURE_EDITING
	line_input_state = new_line_input_t(FOR_SHELL);
#endif
	/* XXX what should these be while sourcing /etc/profile? */
	global_argc = argc;
	global_argv = argv;
	/* Initialize some more globals to non-zero values */
	set_cwd();
#if ENABLE_HUSH_INTERACTIVE
#if ENABLE_FEATURE_EDITING
	cmdedit_set_initial_prompt();
#endif
	PS2 = "> ";
#endif

	if (EXIT_SUCCESS) /* otherwise is already done */
		last_return_code = EXIT_SUCCESS;

	if (argv[0] && argv[0][0] == '-') {
		debug_printf("sourcing /etc/profile\n");
		input = fopen("/etc/profile", "r");
		if (input != NULL) {
			close_on_exec_on(fileno(input));
			parse_and_run_file(input);
			fclose(input);
		}
	}
	input = stdin;

	while ((opt = getopt(argc, argv, "c:xif")) > 0) {
		switch (opt) {
		case 'c':
			global_argv = argv + optind;
			global_argc = argc - optind;
			opt = parse_and_run_string(optarg, PARSEFLAG_SEMICOLON);
			goto final_return;
		case 'i':
			/* Well, we cannot just declare interactiveness,
			 * we have to have some stuff (ctty, etc) */
			/* interactive_fd++; */
			break;
		case 'f':
			fake_mode = 1;
			break;
		default:
#ifndef BB_VER
			fprintf(stderr, "Usage: sh [FILE]...\n"
					"   or: sh -c command [args]...\n\n");
			exit(EXIT_FAILURE);
#else
			bb_show_usage();
#endif
		}
	}
#if ENABLE_HUSH_JOB
	/* A shell is interactive if the '-i' flag was given, or if all of
	 * the following conditions are met:
	 *    no -c command
	 *    no arguments remaining or the -s flag given
	 *    standard input is a terminal
	 *    standard output is a terminal
	 *    Refer to Posix.2, the description of the 'sh' utility. */
	if (argv[optind] == NULL && input == stdin
	 && isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)
	) {
		saved_tty_pgrp = tcgetpgrp(STDIN_FILENO);
		debug_printf("saved_tty_pgrp=%d\n", saved_tty_pgrp);
		if (saved_tty_pgrp >= 0) {
			/* try to dup to high fd#, >= 255 */
			interactive_fd = fcntl(STDIN_FILENO, F_DUPFD, 255);
			if (interactive_fd < 0) {
				/* try to dup to any fd */
				interactive_fd = dup(STDIN_FILENO);
				if (interactive_fd < 0)
					/* give up */
					interactive_fd = 0;
			}
			// TODO: track & disallow any attempts of user
			// to (inadvertently) close/redirect it
		}
	}
	debug_printf("interactive_fd=%d\n", interactive_fd);
	if (interactive_fd) {
		/* Looks like they want an interactive shell */
		setup_job_control();
		/* Make xfuncs do cleanup on exit */
		die_sleep = -1; /* flag */
// FIXME: should we reset die_sleep = 0 whereever we fork?
		if (setjmp(die_jmp)) {
			/* xfunc has failed! die die die */
			hush_exit(xfunc_error_retval);
		}
#if !ENABLE_FEATURE_SH_EXTRA_QUIET
		printf("\n\n%s hush - the humble shell v"HUSH_VER_STR"\n", bb_banner);
		printf("Enter 'help' for a list of built-in commands.\n\n");
#endif
	}
#elif ENABLE_HUSH_INTERACTIVE
/* no job control compiled, only prompt/line editing */
	if (argv[optind] == NULL && input == stdin
	 && isatty(STDIN_FILENO) && isatty(STDOUT_FILENO)
	) {
		interactive_fd = fcntl(STDIN_FILENO, F_DUPFD, 255);
		if (interactive_fd < 0) {
			/* try to dup to any fd */
			interactive_fd = dup(STDIN_FILENO);
			if (interactive_fd < 0)
				/* give up */
				interactive_fd = 0;
		}
	}

#endif

	if (argv[optind] == NULL) {
		opt = parse_and_run_file(stdin);
	} else {
		debug_printf("\nrunning script '%s'\n", argv[optind]);
		global_argv = argv + optind;
		global_argc = argc - optind;
		input = xfopen(argv[optind], "r");
		fcntl(fileno(input), F_SETFD, FD_CLOEXEC);
		opt = parse_and_run_file(input);
	}

 final_return:

#if ENABLE_FEATURE_CLEAN_UP
	fclose(input);
	if (cwd != bb_msg_unknown)
		free((char*)cwd);
	cur_var = top_var->next;
	while (cur_var) {
		struct variable *tmp = cur_var;
		if (!cur_var->max_len)
			free(cur_var->varstr);
		cur_var = cur_var->next;
		free(tmp);
	}
#endif
	hush_exit(opt ? opt : last_return_code);
}


#if ENABLE_LASH
int lash_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int lash_main(int argc, char **argv)
{
	//bb_error_msg("lash is deprecated, please use hush instead");
	return hush_main(argc, argv);
}
#endif
