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
 *      o_addchr() derived from similar w_addchar function in glibc-2.2.
 *      setup_redirect(), redirect_opt_num(), and big chunks of main()
 *      and many builtins derived from contributions by Erik Andersen
 *      miscellaneous bugfixes from Matt Kraai.
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
 *      change { and } from special chars to reserved words
 *      builtins: break, continue, eval, return, set, trap, ulimit
 *      test magic exec
 *      check setting of global_argc and global_argv
 *      follow IFS rules more precisely, including update semantics
 *      figure out what to do with backslash-newline
 *      propagate syntax errors, die on resource errors?
 *      continuation lines, both explicit and implicit - done?
 *      memory leak finding and plugging - done?
 *      maybe change charmap[] to use 2-bit entries
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */


#include <glob.h>      /* glob, of course */
/* #include <dmalloc.h> */

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
#define debug_printf_glob(...)   do {} while (0)
#define debug_printf_list(...)   do {} while (0)
#define debug_printf_subst(...)  do {} while (0)
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
#define DEBUG_JOBS 1
#else
#define DEBUG_JOBS 0
#endif

#ifndef debug_printf_expand
#define debug_printf_expand(...) fprintf(stderr, __VA_ARGS__)
#define DEBUG_EXPAND 1
#else
#define DEBUG_EXPAND 0
#endif

#ifndef debug_printf_glob
#define debug_printf_glob(...) fprintf(stderr, __VA_ARGS__)
#define DEBUG_GLOB 1
#else
#define DEBUG_GLOB 0
#endif

#ifndef debug_printf_list
#define debug_printf_list(...) fprintf(stderr, __VA_ARGS__)
#endif

#ifndef debug_printf_subst
#define debug_printf_subst(...) fprintf(stderr, __VA_ARGS__)
#endif

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

#if DEBUG_EXPAND
static void debug_print_strings(const char *prefix, char **vv)
{
	fprintf(stderr, "%s:\n", prefix);
	while (*vv)
		fprintf(stderr, " '%s'\n", *vv++);
}
#else
#define debug_print_strings(prefix, vv) ((void)0)
#endif

/*
 * Leak hunting. Use hush_leaktool.sh for post-processing.
 */
#ifdef FOR_HUSH_LEAKTOOL
/* suppress "warning: no previous prototype..." */
void *xxmalloc(int lineno, size_t size);
void *xxrealloc(int lineno, void *ptr, size_t size);
char *xxstrdup(int lineno, const char *str);
void xxfree(void *ptr);
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


/* Keep unconditionally on for now */
#define HUSH_DEBUG 1
/* Do we support ANY keywords? */
#if ENABLE_HUSH_IF || ENABLE_HUSH_LOOPS
#define HAS_KEYWORDS 1
#define IF_HAS_KEYWORDS(...) __VA_ARGS__
#define IF_HAS_NO_KEYWORDS(...)
#else
#define HAS_KEYWORDS 0
#define IF_HAS_KEYWORDS(...)
#define IF_HAS_NO_KEYWORDS(...) __VA_ARGS__
#endif


#define SPECIAL_VAR_SYMBOL       3
#define PARSEFLAG_EXIT_FROM_LOOP 1

typedef enum {
	REDIRECT_INPUT     = 1,
	REDIRECT_OVERWRITE = 2,
	REDIRECT_APPEND    = 3,
	REDIRECT_HEREIS    = 4,
	REDIRECT_IO        = 5
} redir_type;

/* The descrip member of this structure is only used to make
 * debugging output pretty */
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

/* This holds pointers to the various results of parsing */
struct p_context {
	struct child_prog *child;
	struct pipe *list_head;
	struct pipe *pipe;
	struct redir_struct *pending_redirect;
#if HAS_KEYWORDS
	smallint ctx_res_w;
	smallint ctx_inverted; /* "! cmd | cmd" */
	int old_flag; /* bitmask of FLAG_xxx, for figuring out valid reserved words */
	struct p_context *stack;
#endif
};

struct redir_struct {
	struct redir_struct *next;
	char *rd_filename;          /* filename */
	int fd;                     /* file descriptor being redirected */
	int dup;                    /* -1, or file descriptor being duplicated */
	smallint /*enum redir_type*/ rd_type;
};

struct child_prog {
	pid_t pid;                  /* 0 if exited */
	smallint is_stopped;        /* is the program currently running? */
	smallint subshell;          /* flag, non-zero if group must be forked */
	char **argv;                /* program name and arguments */
	struct pipe *group;         /* if non-NULL, first in group or subshell */
	struct redir_struct *redirects; /* I/O redirections */
	struct pipe *family;        /* pointer back to the child's parent pipe */
};
/* argv vector may contain variable references (^Cvar^C, ^C0^C etc)
 * and on execution these are substituted with their values.
 * Substitution can make _several_ words out of one argv[n]!
 * Example: argv[0]=='.^C*^C.' here: echo .$*.
 * References of the form ^C`cmd arg^C are `cmd arg` substitutions.
 */

struct pipe {
	struct pipe *next;
	int num_progs;              /* total number of programs in job */
	int alive_progs;            /* number of programs running (not exited) */
	int stopped_progs;          /* number of programs alive, but stopped */
#if ENABLE_HUSH_JOB
	int jobid;                  /* job number */
	pid_t pgrp;                 /* process group ID for the job */
	char *cmdtext;              /* name of job */
#endif
	struct child_prog *progs;   /* array of commands in pipe */
	smallint followup;          /* PIPE_BG, PIPE_SEQ, PIPE_OR, PIPE_AND */
	IF_HAS_KEYWORDS(smallint pi_inverted;) /* "! cmd | cmd" */
	IF_HAS_KEYWORDS(smallint res_word;) /* needed for if, for, while, until... */
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
	/* Misnomer! it's not "quoting", it's "protection against globbing"!
	 * (by prepending \ to *, ?, [ and to \ too) */
	smallint o_quote;
	smallint o_glob;
	smallint nonnull;
	smallint has_empty_slot;
	smallint o_assignment; /* 0:maybe, 1:yes, 2:no */
} o_string;
enum {
	MAYBE_ASSIGNMENT = 0,
	DEFINITELY_ASSIGNMENT = 1,
	NOT_ASSIGNMENT = 2,
};
/* Used for initialization: o_string foo = NULL_O_STRING; */
#define NULL_O_STRING { NULL }

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
#define i_getch(input) ((input)->get(input))
#define i_peek(input) ((input)->peek(input))

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
	pid_t root_pid;
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
#define root_pid         (G.root_pid        )
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
#define INIT_G() do { \
	SET_PTR_TO_GLOBALS(xzalloc(sizeof(G))); \
} while (0)


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
#if BB_MMU
#define pseudo_exec_argv(ptrs2free, argv)  pseudo_exec_argv(argv)
#define      pseudo_exec(ptrs2free, child)      pseudo_exec(child)
#endif
static void pseudo_exec_argv(char **ptrs2free, char **argv) ATTRIBUTE_NORETURN;
static void pseudo_exec(char **ptrs2free, struct child_prog *child) ATTRIBUTE_NORETURN;
static int run_pipe(struct pipe *pi);
/*   data structure manipulation: */
static int setup_redirect(struct p_context *ctx, int fd, redir_type style, struct in_str *input);
static void initialize_context(struct p_context *ctx);
static int done_word(o_string *dest, struct p_context *ctx);
static int done_command(struct p_context *ctx);
static void done_pipe(struct p_context *ctx, pipe_style type);
/*   primary string parsing: */
static int redirect_dup_num(struct in_str *input);
static int redirect_opt_num(o_string *o);
#if ENABLE_HUSH_TICK
static int process_command_subs(o_string *dest,
		struct in_str *input, const char *subst_end);
#endif
static int parse_group(o_string *dest, struct p_context *ctx, struct in_str *input, int ch);
static const char *lookup_param(const char *src);
static int handle_dollar(o_string *dest,
		struct in_str *input);
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


static int glob_needed(const char *s)
{
	while (*s) {
		if (*s == '\\')
			s++;
		if (*s == '*' || *s == '[' || *s == '?')
			return 1;
		s++;
	}
	return 0;
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

/* Replace each \x with x in place, return ptr past NUL. */
static char *unbackslash(char *src)
{
	char *dst = src;
	while (1) {
		if (*src == '\\')
			src++;
		if ((*dst++ = *src++) == '\0')
			break;
	}
	return dst;
}

static char **add_malloced_strings_to_strings(char **strings, char **add)
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
		v[count1 + i] = add[i];
	return v;
}

static char **add_malloced_string_to_strings(char **strings, char *add)
{
	char *v[2];

	v[0] = add;
	v[1] = NULL;

	return add_malloced_strings_to_strings(strings, v);
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

#if !BB_MMU
#define EXTRA_PTRS 5 /* 1 for NULL, 1 for args, 3 for paranoid reasons */
static char **alloc_ptrs(char **argv)
{
	char **v = argv;
	while (*v)
		v++;
	return xzalloc((v - argv + EXTRA_PTRS) * sizeof(v[0]));
}
#endif


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
static int builtin_true(char **argv);
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
	const char *cmd;
	int (*function)(char **argv);
#if ENABLE_HUSH_HELP
	const char *descr;
#define BLTIN(cmd, func, help) { cmd, func, help }
#else
#define BLTIN(cmd, func, help) { cmd, func }
#endif
};

/* For now, echo and test are unconditionally enabled.
 * Maybe make it configurable? */
static const struct built_in_command bltins[] = {
	BLTIN("."     , builtin_source, "Run commands in a file"),
	BLTIN(":"     , builtin_true, "No-op"),
	BLTIN("["     , builtin_test, "Test condition"),
	BLTIN("[["    , builtin_test, "Test condition"),
#if ENABLE_HUSH_JOB
	BLTIN("bg"    , builtin_fg_bg, "Resume a job in the background"),
#endif
//	BLTIN("break" , builtin_not_written, "Exit for, while or until loop"),
	BLTIN("cd"    , builtin_cd, "Change directory"),
//	BLTIN("continue", builtin_not_written, "Continue for, while or until loop"),
	BLTIN("echo"  , builtin_echo, "Write strings to stdout"),
	BLTIN("eval"  , builtin_eval, "Construct and run shell command"),
	BLTIN("exec"  , builtin_exec, "Execute command, don't return to shell"),
	BLTIN("exit"  , builtin_exit, "Exit"),
	BLTIN("export", builtin_export, "Set environment variable"),
#if ENABLE_HUSH_JOB
	BLTIN("fg"    , builtin_fg_bg, "Bring job into the foreground"),
	BLTIN("jobs"  , builtin_jobs, "List active jobs"),
#endif
	BLTIN("pwd"   , builtin_pwd, "Print current directory"),
	BLTIN("read"  , builtin_read, "Input environment variable"),
//	BLTIN("return", builtin_not_written, "Return from a function"),
	BLTIN("set"   , builtin_set, "Set/unset shell local variables"),
	BLTIN("shift" , builtin_shift, "Shift positional parameters"),
//	BLTIN("trap"  , builtin_not_written, "Trap signals"),
	BLTIN("test"  , builtin_test, "Test condition"),
//	BLTIN("ulimit", builtin_not_written, "Control resource limits"),
	BLTIN("umask" , builtin_umask, "Set file creation mask"),
	BLTIN("unset" , builtin_unset, "Unset environment variable"),
#if ENABLE_HUSH_HELP
	BLTIN("help"  , builtin_help, "List shell built-in commands"),
#endif
};


/* Signals are grouped, we handle them in batches */
static void set_misc_sighandler(void (*handler)(int))
{
	bb_signals(0
		+ (1 << SIGINT)
		+ (1 << SIGQUIT)
		+ (1 << SIGTERM)
		, handler);
}

#if ENABLE_HUSH_JOB

static void set_fatal_sighandler(void (*handler)(int))
{
	bb_signals(0
		+ (1 << SIGILL)
		+ (1 << SIGTRAP)
		+ (1 << SIGABRT)
		+ (1 << SIGFPE)
		+ (1 << SIGBUS)
		+ (1 << SIGSEGV)
	/* bash 3.2 seems to handle these just like 'fatal' ones */
		+ (1 << SIGHUP)
		+ (1 << SIGPIPE)
		+ (1 << SIGALRM)
		, handler);
}
static void set_jobctrl_sighandler(void (*handler)(int))
{
	bb_signals(0
		+ (1 << SIGTSTP)
		+ (1 << SIGTTIN)
		+ (1 << SIGTTOU)
		, handler);
}
/* SIGCHLD is special and handled separately */

static void set_every_sighandler(void (*handler)(int))
{
	set_fatal_sighandler(handler);
	set_jobctrl_sighandler(handler);
	set_misc_sighandler(handler);
	signal(SIGCHLD, handler);
}

static void handler_ctrl_c(int sig ATTRIBUTE_UNUSED)
{
	debug_printf_jobs("got sig %d\n", sig);
// as usual we can have all kinds of nasty problems with leaked malloc data here
	siglongjmp(toplevel_jb, 1);
}

static void handler_ctrl_z(int sig ATTRIBUTE_UNUSED)
{
	pid_t pid;

	debug_printf_jobs("got tty sig %d in pid %d\n", sig, getpid());
	pid = fork();
	if (pid < 0) /* can't fork. Pretend there was no ctrl-Z */
		return;
	ctrl_z_flag = 1;
	if (!pid) { /* child */
		if (ENABLE_HUSH_JOB)
			die_sleep = 0; /* let nofork's xfuncs die */
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
	/* Disable all signals: job control, SIGPIPE, etc. */
	sigprocmask_allsigs(SIG_BLOCK);

	if (interactive_fd)
		tcsetpgrp(interactive_fd, saved_tty_pgrp);

	/* Not a signal, just exit */
	if (sig <= 0)
		_exit(- sig);

	kill_myself_with_sig(sig); /* does not return */
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


/*
 * o_string support
 */
#define B_CHUNK  (32 * sizeof(char*))

static void o_reset(o_string *o)
{
	o->length = 0;
	o->nonnull = 0;
	if (o->data)
		o->data[0] = '\0';
}

static void o_free(o_string *o)
{
	free(o->data);
	memset(o, 0, sizeof(*o));
}

static void o_grow_by(o_string *o, int len)
{
	if (o->length + len > o->maxlen) {
		o->maxlen += (2*len > B_CHUNK ? 2*len : B_CHUNK);
		o->data = xrealloc(o->data, 1 + o->maxlen);
	}
}

static void o_addchr(o_string *o, int ch)
{
	debug_printf("o_addchr: '%c' o->length=%d o=%p\n", ch, o->length, o);
	o_grow_by(o, 1);
	o->data[o->length] = ch;
	o->length++;
	o->data[o->length] = '\0';
}

static void o_addstr(o_string *o, const char *str, int len)
{
	o_grow_by(o, len);
	memcpy(&o->data[o->length], str, len);
	o->length += len;
	o->data[o->length] = '\0';
}

static void o_addstr_duplicate_backslash(o_string *o, const char *str, int len)
{
	while (len) {
		o_addchr(o, *str);
		if (*str++ == '\\'
		 && (*str != '*' && *str != '?' && *str != '[')
		) {
			o_addchr(o, '\\');
		}
		len--;
	}
}

/* My analysis of quoting semantics tells me that state information
 * is associated with a destination, not a source.
 */
static void o_addqchr(o_string *o, int ch)
{
	int sz = 1;
	if (strchr("*?[\\", ch)) {
		sz++;
		o->data[o->length] = '\\';
		o->length++;
	}
	o_grow_by(o, sz);
	o->data[o->length] = ch;
	o->length++;
	o->data[o->length] = '\0';
}

static void o_addQchr(o_string *o, int ch)
{
	int sz = 1;
	if (o->o_quote && strchr("*?[\\", ch)) {
		sz++;
		o->data[o->length] = '\\';
		o->length++;
	}
	o_grow_by(o, sz);
	o->data[o->length] = ch;
	o->length++;
	o->data[o->length] = '\0';
}

static void o_addQstr(o_string *o, const char *str, int len)
{
	if (!o->o_quote) {
		o_addstr(o, str, len);
		return;
	}
	while (len) {
		char ch;
		int sz;
		int ordinary_cnt = strcspn(str, "*?[\\");
		if (ordinary_cnt > len) /* paranoia */
			ordinary_cnt = len;
		o_addstr(o, str, ordinary_cnt);
		if (ordinary_cnt == len)
			return;
		str += ordinary_cnt;
		len -= ordinary_cnt + 1; /* we are processing + 1 char below */

		ch = *str++;
		sz = 1;
		if (ch) { /* it is necessarily one of "*?[\\" */
			sz++;
			o->data[o->length] = '\\';
			o->length++;
		}
		o_grow_by(o, sz);
		o->data[o->length] = ch;
		o->length++;
		o->data[o->length] = '\0';
	}
}

/* A special kind of o_string for $VAR and `cmd` expansion.
 * It contains char* list[] at the beginning, which is grown in 16 element
 * increments. Actual string data starts at the next multiple of 16 * (char*).
 * list[i] contains an INDEX (int!) into this string data.
 * It means that if list[] needs to grow, data needs to be moved higher up
 * but list[i]'s need not be modified.
 * NB: remembering how many list[i]'s you have there is crucial.
 * o_finalize_list() operation post-processes this structure - calculates
 * and stores actual char* ptrs in list[]. Oh, it NULL terminates it as well.
 */
#if DEBUG_EXPAND || DEBUG_GLOB
static void debug_print_list(const char *prefix, o_string *o, int n)
{
	char **list = (char**)o->data;
	int string_start = ((n + 0xf) & ~0xf) * sizeof(list[0]);
	int i = 0;
	fprintf(stderr, "%s: list:%p n:%d string_start:%d length:%d maxlen:%d\n",
			prefix, list, n, string_start, o->length, o->maxlen);
	while (i < n) {
		fprintf(stderr, " list[%d]=%d '%s' %p\n", i, (int)list[i],
				o->data + (int)list[i] + string_start,
				o->data + (int)list[i] + string_start);
		i++;
	}
	if (n) {
		const char *p = o->data + (int)list[n - 1] + string_start;
		fprintf(stderr, " total_sz:%d\n", (p + strlen(p) + 1) - o->data);
	}
}
#else
#define debug_print_list(prefix, o, n) ((void)0)
#endif

/* n = o_save_ptr_helper(str, n) "starts new string" by storing an index value
 * in list[n] so that it points past last stored byte so far.
 * It returns n+1. */
static int o_save_ptr_helper(o_string *o, int n)
{
	char **list = (char**)o->data;
	int string_start;
	int string_len;

	if (!o->has_empty_slot) {
		string_start = ((n + 0xf) & ~0xf) * sizeof(list[0]);
		string_len = o->length - string_start;
		if (!(n & 0xf)) { /* 0, 0x10, 0x20...? */
			debug_printf_list("list[%d]=%d string_start=%d (growing)\n", n, string_len, string_start);
			/* list[n] points to string_start, make space for 16 more pointers */
			o->maxlen += 0x10 * sizeof(list[0]);
			o->data = xrealloc(o->data, o->maxlen + 1);
			list = (char**)o->data;
			memmove(list + n + 0x10, list + n, string_len);
			o->length += 0x10 * sizeof(list[0]);
		} else
			debug_printf_list("list[%d]=%d string_start=%d\n", n, string_len, string_start);
	} else {
		/* We have empty slot at list[n], reuse without growth */
		string_start = ((n+1 + 0xf) & ~0xf) * sizeof(list[0]); /* NB: n+1! */
		string_len = o->length - string_start;
		debug_printf_list("list[%d]=%d string_start=%d (empty slot)\n", n, string_len, string_start);
		o->has_empty_slot = 0;
	}
	list[n] = (char*)(ptrdiff_t)string_len;
	return n + 1;
}

/* "What was our last o_save_ptr'ed position (byte offset relative o->data)?" */
static int o_get_last_ptr(o_string *o, int n)
{
	char **list = (char**)o->data;
	int string_start = ((n + 0xf) & ~0xf) * sizeof(list[0]);

	return ((int)(ptrdiff_t)list[n-1]) + string_start;
}

/* o_glob performs globbing on last list[], saving each result
 * as a new list[]. */
static int o_glob(o_string *o, int n)
{
	glob_t globdata;
	int gr;
	char *pattern;

	debug_printf_glob("start o_glob: n:%d o->data:%p\n", n, o->data);
	if (!o->data)
		return o_save_ptr_helper(o, n);
	pattern = o->data + o_get_last_ptr(o, n);
	debug_printf_glob("glob pattern '%s'\n", pattern);
	if (!glob_needed(pattern)) {
 literal:
		o->length = unbackslash(pattern) - o->data;
		debug_printf_glob("glob pattern '%s' is literal\n", pattern);
		return o_save_ptr_helper(o, n);
	}

	memset(&globdata, 0, sizeof(globdata));
	gr = glob(pattern, 0, NULL, &globdata);
	debug_printf_glob("glob('%s'):%d\n", pattern, gr);
	if (gr == GLOB_NOSPACE)
		bb_error_msg_and_die("out of memory during glob");
	if (gr == GLOB_NOMATCH) {
		globfree(&globdata);
		goto literal;
	}
	if (gr != 0) { /* GLOB_ABORTED ? */
//TODO: testcase for bad glob pattern behavior
		bb_error_msg("glob(3) error %d on '%s'", gr, pattern);
	}
	if (globdata.gl_pathv && globdata.gl_pathv[0]) {
		char **argv = globdata.gl_pathv;
		o->length = pattern - o->data; /* "forget" pattern */
		while (1) {
			o_addstr(o, *argv, strlen(*argv) + 1);
			n = o_save_ptr_helper(o, n);
			argv++;
			if (!*argv)
				break;
		}
	}
	globfree(&globdata);
	if (DEBUG_GLOB)
		debug_print_list("o_glob returning", o, n);
	return n;
}

/* If o->o_glob == 1, glob the string so far remembered.
 * Otherwise, just finish current list[] and start new */
static int o_save_ptr(o_string *o, int n)
{
	if (o->o_glob)
		return o_glob(o, n); /* o_save_ptr_helper is inside */
	return o_save_ptr_helper(o, n);
}

/* "Please convert list[n] to real char* ptrs, and NULL terminate it." */
static char **o_finalize_list(o_string *o, int n)
{
	char **list;
	int string_start;

	n = o_save_ptr(o, n); /* force growth for list[n] if necessary */
	if (DEBUG_EXPAND)
		debug_print_list("finalized", o, n);
	debug_printf_expand("finalized n:%d\n", n);
	list = (char**)o->data;
	string_start = ((n + 0xf) & ~0xf) * sizeof(list[0]);
	list[--n] = NULL;
	while (n) {
		n--;
		list[n] = o->data + (int)(ptrdiff_t)list[n] + string_start;
	}
	return list;
}


/*
 * in_str support
 */
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
	 * is actually being read */
	do {
		r = read_line_input(prompt_str, user_input_buf, BUFSIZ-1, line_input_state);
	} while (r == 0); /* repeat if Ctrl-C */
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
		if (redir->dup == -1 && redir->rd_filename == NULL) {
			/* something went wrong in the parse.  Pretend it didn't happen */
			continue;
		}
		if (redir->dup == -1) {
			char *p;
			mode = redir_table[redir->rd_type].mode;
//TODO: check redir to names like '\\'
			p = expand_string_to_string(redir->rd_filename);
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
static void pseudo_exec_argv(char **ptrs2free, char **argv)
{
	int i, rcode;
	char *p;
	const struct built_in_command *x;

	for (i = 0; is_assignment(argv[i]); i++) {
		debug_printf_exec("pid %d environment modification: %s\n",
				getpid(), argv[i]);
		p = expand_string_to_string(argv[i]);
#if !BB_MMU
		*ptrs2free++ = p;
#endif
		putenv(p);
	}
	argv += i;
	/* If a variable is assigned in a forest, and nobody listens,
	 * was it ever really set?
	 */
	if (!argv[0])
		_exit(EXIT_SUCCESS);

	argv = expand_strvec_to_strvec(argv);
#if !BB_MMU
	*ptrs2free++ = (char*) argv;
#endif

	/*
	 * Check if the command matches any of the builtins.
	 * Depending on context, this might be redundant.  But it's
	 * easier to waste a few CPU cycles than it is to figure out
	 * if this is one of those cases.
	 */
	for (x = bltins; x != &bltins[ARRAY_SIZE(bltins)]; x++) {
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
	_exit(EXIT_FAILURE);
}

/* Called after [v]fork() in run_pipe()
 */
static void pseudo_exec(char **ptrs2free, struct child_prog *child)
{
	if (child->argv)
		pseudo_exec_argv(ptrs2free, child->argv);

	if (child->group) {
#if !BB_MMU
		bb_error_msg_and_die("nested lists are not supported on NOMMU");
#else
		int rcode;
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
	if (!argv || !argv[0]) {
		pi->cmdtext = xzalloc(1);
		return pi->cmdtext;
	}

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

/* Remove a backgrounded job */
static void delete_finished_bg_job(struct pipe *pi)
{
	remove_bg_job(pi);
	pi->stopped_progs = 0;
	free_pipe(pi, 0);
	free(pi);
}
#endif /* JOB */

/* Check to see if any processes have exited -- if they
 * have, figure out why and see if a job has completed */
static int checkjobs(struct pipe* fg_pipe)
{
	int attributes;
	int status;
#if ENABLE_HUSH_JOB
	struct pipe *pi;
#endif
	pid_t childpid;
	int rcode = 0;

	attributes = WUNTRACED;
	if (fg_pipe == NULL)
		attributes |= WNOHANG;

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
		int i;
		const int dead = WIFEXITED(status) || WIFSIGNALED(status);
#if DEBUG_JOBS
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
			for (i = 0; i < fg_pipe->num_progs; i++) {
				debug_printf_jobs("check pid %d\n", fg_pipe->progs[i].pid);
				if (fg_pipe->progs[i].pid != childpid)
					continue;
				/* printf("process %d exit %d\n", i, WEXITSTATUS(status)); */
				if (dead) {
					fg_pipe->progs[i].pid = 0;
					fg_pipe->alive_progs--;
					if (i == fg_pipe->num_progs - 1) {
						/* last process gives overall exitstatus */
						rcode = WEXITSTATUS(status);
						IF_HAS_KEYWORDS(if (fg_pipe->pi_inverted) rcode = !rcode;)
					}
				} else {
					fg_pipe->progs[i].is_stopped = 1;
					fg_pipe->stopped_progs++;
				}
				debug_printf_jobs("fg_pipe: alive_progs %d stopped_progs %d\n",
						fg_pipe->alive_progs, fg_pipe->stopped_progs);
				if (fg_pipe->alive_progs - fg_pipe->stopped_progs <= 0) {
					/* All processes in fg pipe have exited/stopped */
#if ENABLE_HUSH_JOB
					if (fg_pipe->alive_progs)
						insert_bg_job(fg_pipe);
#endif
					return rcode;
				}
				/* There are still running processes in the fg pipe */
				goto wait_more; /* do waitpid again */
			}
			/* it wasnt fg_pipe, look for process in bg pipes */
		}

#if ENABLE_HUSH_JOB
		/* We asked to wait for bg or orphaned children */
		/* No need to remember exitcode in this case */
		for (pi = job_list; pi; pi = pi->next) {
			for (i = 0; i < pi->num_progs; i++) {
				if (pi->progs[i].pid == childpid)
					goto found_pi_and_prognum;
			}
		}
		/* Happens when shell is used as init process (init=/bin/sh) */
		debug_printf("checkjobs: pid %d was not in our list!\n", childpid);
		continue; /* do waitpid again */

 found_pi_and_prognum:
		if (dead) {
			/* child exited */
			pi->progs[i].pid = 0;
			pi->alive_progs--;
			if (!pi->alive_progs) {
				printf(JOB_STATUS_FORMAT, pi->jobid,
						"Done", pi->cmdtext);
				delete_finished_bg_job(pi);
			}
		} else {
			/* child stopped */
			pi->progs[i].is_stopped = 1;
			pi->stopped_progs++;
		}
#endif
	} /* while (waitpid succeeds)... */

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
	tcsetpgrp(interactive_fd, p);
//	if (tcsetpgrp(interactive_fd, p) && errno != ENOTTY)
//		bb_perror_msg("tcsetpgrp-4a");
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
	pi->alive_progs = 1;
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
		IF_HAS_KEYWORDS(if (pi->pi_inverted) rcode = !rcode;)
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
			putenv(p);
//FIXME: do we leak p?!
		}
		for (x = bltins; x != &bltins[ARRAY_SIZE(bltins)]; x++) {
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
				argv_expanded = expand_strvec_to_strvec(argv + i);
				rcode = x->function(argv_expanded) & 0xff;
				free(argv_expanded);
				restore_redirects(squirrel);
				debug_printf_exec("run_pipe return %d\n", rcode);
				IF_HAS_KEYWORDS(if (pi->pi_inverted) rcode = !rcode;)
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
				argv_expanded = expand_strvec_to_strvec(argv + i);
				debug_printf_exec(": run_nofork_applet '%s' '%s'...\n", argv_expanded[0], argv_expanded[1]);
				rcode = run_nofork_applet_prime(&nofork_save, a, argv_expanded);
				free(argv_expanded);
				restore_redirects(squirrel);
				debug_printf_exec("run_pipe return %d\n", rcode);
				IF_HAS_KEYWORDS(if (pi->pi_inverted) rcode = !rcode;)
				return rcode;
			}
		}
#endif
	}

	/* Disable job control signals for shell (parent) and
	 * for initial child code after fork */
	set_jobctrl_sighandler(SIG_IGN);

	/* Going to fork a child per each pipe member */
	pi->alive_progs = 0;
	nextin = 0;

	for (i = 0; i < pi->num_progs; i++) {
#if !BB_MMU
		char **ptrs2free = NULL;
#endif
		child = &(pi->progs[i]);
		if (child->argv) {
			debug_printf_exec(": pipe member '%s' '%s'...\n", child->argv[0], child->argv[1]);
#if !BB_MMU
			ptrs2free = alloc_ptrs(child->argv);
#endif
		} else
			debug_printf_exec(": pipe member with no argv\n");

		/* pipes are inserted between pairs of commands */
		pipefds[0] = 0;
		pipefds[1] = 1;
		if ((i + 1) < pi->num_progs)
			xpipe(pipefds);

		child->pid = BB_MMU ? fork() : vfork();
		if (!child->pid) { /* child */
			if (ENABLE_HUSH_JOB)
				die_sleep = 0; /* let nofork's xfuncs die */
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
			pseudo_exec(ptrs2free, child); /* does not return */
		}
#if !BB_MMU
		free_strings(ptrs2free);
#endif
		if (child->pid < 0) { /* [v]fork failed */
			/* Clearly indicate, was it fork or vfork */
			bb_perror_msg(BB_MMU ? "fork" : "vfork");
		} else {
			pi->alive_progs++;
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

	if (!pi->alive_progs) {
		debug_printf_exec("run_pipe return 1 (all forks failed, no children)\n");
		return 1;
	}

	debug_printf_exec("run_pipe return -1 (%u children started)\n", pi->alive_progs);
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
	reserved_style rword IF_HAS_NO_KEYWORDS(= RES_NONE);
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
			syntax("malformed for"); /* 2nd condition: FOR not followed by IN */
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
		signal_SA_RESTART_empty_mask(SIGTSTP, handler_ctrl_z);
		signal(SIGINT, handler_ctrl_c);
	}
#endif /* JOB */

	for (; pi; pi = flag_restore ? rpipe : pi->next) {
		IF_HAS_KEYWORDS(rword = pi->res_word;)
		IF_HAS_NO_KEYWORDS(rword = RES_NONE;)
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
				debug_print_strings("for_list made from", pi->next->progs->argv);
				for_list = expand_strvec_to_strvec(pi->next->progs->argv);
				debug_print_strings("for_list", for_list);
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
			debug_printf_clean("%s   redirect %d%s", indenter(indent), r->fd, redir_table[r->rd_type].descrip);
			if (r->dup == -1) {
				/* guard against the case >$FOO, where foo is unset or blank */
				if (r->rd_filename) {
					debug_printf_clean(" %s\n", r->rd_filename);
					free(r->rd_filename);
					r->rd_filename = NULL;
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
#if HAS_KEYWORDS
		debug_printf_clean("%s pipe reserved mode %d\n", indenter(indent), pi->res_word);
#endif
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
	debug_printf_exec("run_and_free_list return %d\n", rcode);
	return rcode;
}


/* expand_strvec_to_strvec() takes a list of strings, expands
 * all variable references within and returns a pointer to
 * a list of expanded strings, possibly with larger number
 * of strings. (Think VAR="a b"; echo $VAR).
 * This new list is allocated as a single malloc block.
 * NULL-terminated list of char* pointers is at the beginning of it,
 * followed by strings themself.
 * Caller can deallocate entire list by single free(list). */

/* Store given string, finalizing the word and starting new one whenever
 * we encounter ifs char(s). This is used for expanding variable values.
 * End-of-string does NOT finalize word: think about 'echo -$VAR-' */
static int expand_on_ifs(o_string *output, int n, const char *str)
{
	while (1) {
		int word_len = strcspn(str, ifs);
		if (word_len) {
			if (output->o_quote || !output->o_glob)
				o_addQstr(output, str, word_len);
			else /* protect backslashes against globbing up :) */
				o_addstr_duplicate_backslash(output, str, word_len);
			str += word_len;
		}
		if (!*str)  /* EOL - do not finalize word */
			break;
		o_addchr(output, '\0');
		debug_print_list("expand_on_ifs", output, n);
		n = o_save_ptr(output, n);
		str += strspn(str, ifs); /* skip ifs chars */
	}
	debug_print_list("expand_on_ifs[1]", output, n);
	return n;
}

/* Expand all variable references in given string, adding words to list[]
 * at n, n+1,... positions. Return updated n (so that list[n] is next one
 * to be filled). This routine is extremely tricky: has to deal with
 * variables/parameters with whitespace, $* and $@, and constructs like
 * 'echo -$*-'. If you play here, you must run testsuite afterwards! */
/* NB: another bug is that we cannot detect empty strings yet:
 * "" or $empty"" expands to zero words, has to expand to empty word */
static int expand_vars_to_list(o_string *output, int n, char *arg, char or_mask)
{
	/* or_mask is either 0 (normal case) or 0x80
	 * (expansion of right-hand side of assignment == 1-element expand.
	 * It will also do no globbing, and thus we must not backslash-quote!) */

	char first_ch, ored_ch;
	int i;
	const char *val;
	char *p;

	ored_ch = 0;

	debug_printf_expand("expand_vars_to_list: arg '%s'\n", arg);
	debug_print_list("expand_vars_to_list", output, n);
	n = o_save_ptr(output, n);
	debug_print_list("expand_vars_to_list[0]", output, n);

	while ((p = strchr(arg, SPECIAL_VAR_SYMBOL)) != NULL) {
#if ENABLE_HUSH_TICK
		o_string subst_result = NULL_O_STRING;
#endif

		o_addstr(output, arg, p - arg);
		debug_print_list("expand_vars_to_list[1]", output, n);
		arg = ++p;
		p = strchr(p, SPECIAL_VAR_SYMBOL);

		first_ch = arg[0] | or_mask; /* forced to "quoted" if or_mask = 0x80 */
		ored_ch |= first_ch;
		val = NULL;
		switch (first_ch & 0x7f) {
		/* Highest bit in first_ch indicates that var is double-quoted */
		case '$': /* pid */
			val = utoa(root_pid);
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
				smallint sv = output->o_quote;
				/* unquoted var's contents should be globbed, so don't quote */
				output->o_quote = 0;
				while (global_argv[i]) {
					n = expand_on_ifs(output, n, global_argv[i]);
					debug_printf_expand("expand_vars_to_list: argv %d (last %d)\n", i, global_argc-1);
					if (global_argv[i++][0] && global_argv[i]) {
						/* this argv[] is not empty and not last:
						 * put terminating NUL, start new word */
						o_addchr(output, '\0');
						debug_print_list("expand_vars_to_list[2]", output, n);
						n = o_save_ptr(output, n);
						debug_print_list("expand_vars_to_list[3]", output, n);
					}
				}
				output->o_quote = sv;
			} else
			/* If or_mask is nonzero, we handle assignment 'a=....$@.....'
			 * and in this case should treat it like '$*' - see 'else...' below */
			if (first_ch == ('@'|0x80) && !or_mask) { /* quoted $@ */
				while (1) {
					o_addQstr(output, global_argv[i], strlen(global_argv[i]));
					if (++i >= global_argc)
						break;
					o_addchr(output, '\0');
					debug_print_list("expand_vars_to_list[4]", output, n);
					n = o_save_ptr(output, n);
				}
			} else { /* quoted $*: add as one word */
				while (1) {
					o_addQstr(output, global_argv[i], strlen(global_argv[i]));
					if (!global_argv[++i])
						break;
					if (ifs[0])
						o_addchr(output, ifs[0]);
				}
			}
			break;
		case SPECIAL_VAR_SYMBOL: /* <SPECIAL_VAR_SYMBOL><SPECIAL_VAR_SYMBOL> */
			/* "Empty variable", used to make "" etc to not disappear */
			arg++;
			ored_ch = 0x80;
			break;
#if ENABLE_HUSH_TICK
		case '`': { /* <SPECIAL_VAR_SYMBOL>`cmd<SPECIAL_VAR_SYMBOL> */
			struct in_str input;
			*p = '\0';
			arg++;
//TODO: can we just stuff it into "output" directly?
			debug_printf_subst("SUBST '%s' first_ch %x\n", arg, first_ch);
			setup_string_in_str(&input, arg);
			process_command_subs(&subst_result, &input, NULL);
			debug_printf_subst("SUBST RES '%s'\n", subst_result.data);
			val = subst_result.data;
			goto store_val;
		}
#endif
		default: /* <SPECIAL_VAR_SYMBOL>varname<SPECIAL_VAR_SYMBOL> */
			*p = '\0';
			arg[0] = first_ch & 0x7f;
			if (isdigit(arg[0])) {
				i = xatoi_u(arg);
				if (i < global_argc)
					val = global_argv[i];
				/* else val remains NULL: $N with too big N */
			} else
				val = lookup_param(arg);
			arg[0] = first_ch;
#if ENABLE_HUSH_TICK
 store_val:
#endif
			*p = SPECIAL_VAR_SYMBOL;
			if (!(first_ch & 0x80)) { /* unquoted $VAR */
				debug_printf_expand("unquoted '%s', output->o_quote:%d\n", val, output->o_quote);
				if (val) {
					/* unquoted var's contents should be globbed, so don't quote */
					smallint sv = output->o_quote;
					output->o_quote = 0;
					n = expand_on_ifs(output, n, val);
					val = NULL;
					output->o_quote = sv;
				}
			} else { /* quoted $VAR, val will be appended below */
				debug_printf_expand("quoted '%s', output->o_quote:%d\n", val, output->o_quote);
			}
		}
		if (val) {
			o_addQstr(output, val, strlen(val));
		}

#if ENABLE_HUSH_TICK
		o_free(&subst_result);
#endif
		arg = ++p;
	} /* end of "while (SPECIAL_VAR_SYMBOL is found) ..." */

	if (arg[0]) {
		debug_print_list("expand_vars_to_list[a]", output, n);
		/* this part is literal, and it was already pre-quoted
		 * if needed (much earlier), do not use o_addQstr here! */
		o_addstr(output, arg, strlen(arg) + 1);
		debug_print_list("expand_vars_to_list[b]", output, n);
	} else if (output->length == o_get_last_ptr(output, n) /* expansion is empty */
	 && !(ored_ch & 0x80) /* and all vars were not quoted. */
	) {
		n--;
		/* allow to reuse list[n] later without re-growth */
		output->has_empty_slot = 1;
	} else {
		o_addchr(output, '\0');
	}
	return n;
}

static char **expand_variables(char **argv, int or_mask)
{
	int n;
	char **list;
	char **v;
	o_string output = NULL_O_STRING;

	if (or_mask & 0x100) {
		output.o_quote = 1; /* protect against globbing for "$var" */
		/* (unquoted $var will temporarily switch it off) */
		output.o_glob = 1;
	}

	n = 0;
	v = argv;
	while (*v) {
		n = expand_vars_to_list(&output, n, *v, (char)or_mask);
		v++;
	}
	debug_print_list("expand_variables", &output, n);

	/* output.data (malloced in one block) gets returned in "list" */
	list = o_finalize_list(&output, n);
	debug_print_strings("expand_variables[1]", list);
	return list;
}

static char **expand_strvec_to_strvec(char **argv)
{
	return expand_variables(argv, 0x100);
}

/* Used for expansion of right hand of assignments */
/* NB: should NOT do globbing! "export v=/bin/c*; env | grep ^v=" outputs
 * "v=/bin/c*" */
static char *expand_string_to_string(const char *str)
{
	char *argv[2], **list;

	argv[0] = (char*)str;
	argv[1] = NULL;
	list = expand_variables(argv, 0x80); /* 0x80: make one-element expansion */
	if (HUSH_DEBUG)
		if (!list[0] || list[1])
			bb_error_msg_and_die("BUG in varexp2");
	/* actually, just move string 2*sizeof(char*) bytes back */
	strcpy((char*)list, list[0]);
	debug_printf_expand("string_to_string='%s'\n", (char*)list);
	return (char*)list;
}

/* Used for "eval" builtin */
static char* expand_strvec_to_string(char **argv)
{
	char **list;

	list = expand_variables(argv, 0x80);
	/* Convert all NULs to spaces */
	if (list[0]) {
		int n = 1;
		while (list[n]) {
			if (HUSH_DEBUG)
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


/* Used to get/check local shell variables */
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
	/* redir->rd_filename = NULL; */
	if (last_redir) {
		last_redir->next = redir;
	} else {
		child->redirects = redir;
	}

	redir->rd_type = style;
	redir->fd = (fd == -1) ? redir_table[style].default_fd : fd;

	debug_printf("Redirect type %d%s\n", redir->fd, redir_table[style].descrip);

	/* Check for a '2>&1' type redirect */
	redir->dup = redirect_dup_num(input);
	if (redir->dup == -2)
		return 1;  /* syntax error */
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
	/*pi->followup = 0; - deliberately invalid value */
	/*pi->res_word = RES_NONE; - RES_NONE is 0 anyway */
	return pi;
}

static void initialize_context(struct p_context *ctx)
{
	memset(ctx, 0, sizeof(*ctx));
	ctx->pipe = ctx->list_head = new_pipe();
	/* Create the memory for child, roughly:
	 * ctx->pipe->progs = new struct child_prog;
	 * ctx->pipe->progs[0].family = ctx->pipe;
	 * ctx->child = &ctx->pipe->progs[0];
	 */
	done_command(ctx);
}

/* If a reserved word is found and processed, parse context is modified
 * and 1 is returned.
 * Handles if, then, elif, else, fi, for, while, until, do, done.
 * case, function, and select are obnoxious, save those for later.
 */
#if HAS_KEYWORDS
static int reserved_word(const o_string *word, struct p_context *ctx)
{
	struct reserved_combo {
		char literal[7];
		unsigned char res;
		int flag;
	};
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
	/* Mostly a list of accepted follow-up reserved words.
	 * FLAG_END means we are done with the sequence, and are ready
	 * to turn the compound list into a command.
	 * FLAG_START means the word must start a new compound list.
	 */
	static const struct reserved_combo reserved_list[] = {
#if ENABLE_HUSH_IF
		{ "!",     RES_NONE,  0 },
		{ "if",    RES_IF,    FLAG_THEN | FLAG_START },
		{ "then",  RES_THEN,  FLAG_ELIF | FLAG_ELSE | FLAG_FI },
		{ "elif",  RES_ELIF,  FLAG_THEN },
		{ "else",  RES_ELSE,  FLAG_FI   },
		{ "fi",    RES_FI,    FLAG_END  },
#endif
#if ENABLE_HUSH_LOOPS
		{ "for",   RES_FOR,   FLAG_IN | FLAG_START },
		{ "while", RES_WHILE, FLAG_DO | FLAG_START },
		{ "until", RES_UNTIL, FLAG_DO | FLAG_START },
		{ "in",    RES_IN,    FLAG_DO   },
		{ "do",    RES_DO,    FLAG_DONE },
		{ "done",  RES_DONE,  FLAG_END  }
#endif
	};

	const struct reserved_combo *r;

	for (r = reserved_list;	r < reserved_list + ARRAY_SIZE(reserved_list); r++) {
		if (strcmp(word->data, r->literal) != 0)
			continue;
		debug_printf("found reserved word %s, res %d\n", r->literal, r->res);
		if (r->flag == 0) { /* '!' */
#if ENABLE_HUSH_LOOPS
			if (ctx->ctx_res_w == RES_IN) {
				/* 'for a in ! a b c; ...' - ! isn't a keyword here */
				break;
			}
#endif
			if (ctx->ctx_inverted /* bash doesn't accept '! ! true' */
#if ENABLE_HUSH_LOOPS
			 || ctx->ctx_res_w == RES_FOR /* example: 'for ! a' */
#endif
			) {
				syntax(NULL);
				IF_HAS_KEYWORDS(ctx->ctx_res_w = RES_SNTX;)
			}
			ctx->ctx_inverted = 1;
			return 1;
		}
		if (r->flag & FLAG_START) {
			struct p_context *new;
			debug_printf("push stack\n");
#if ENABLE_HUSH_LOOPS
			if (ctx->ctx_res_w == RES_IN || ctx->ctx_res_w == RES_FOR) {
				syntax("malformed for"); /* example: 'for if' */
				ctx->ctx_res_w = RES_SNTX;
				return 1;
			}
#endif
			new = xmalloc(sizeof(*new));
			*new = *ctx;   /* physical copy */
			initialize_context(ctx);
			ctx->stack = new;
		} else if (ctx->ctx_res_w == RES_NONE || !(ctx->old_flag & (1 << r->res))) {
			syntax(NULL);
			ctx->ctx_res_w = RES_SNTX;
			return 1;
		}
		ctx->ctx_res_w = r->res;
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
		return 1;
	}
	return 0;
}
#endif

/* Word is complete, look at it and update parsing context.
 * Normal return is 0. Syntax errors return 1. */
static int done_word(o_string *word, struct p_context *ctx)
{
	struct child_prog *child = ctx->child;

	/* If this word wasn't an assignment, next ones definitely
	 * can't be assignments. Even if they look like ones. */
	if (word->o_assignment != DEFINITELY_ASSIGNMENT) {
		word->o_assignment = NOT_ASSIGNMENT;
	} else {
		word->o_assignment = MAYBE_ASSIGNMENT;
	}

	debug_printf_parse("done_word entered: '%s' %p\n", word->data, child);
	if (word->length == 0 && word->nonnull == 0) {
		debug_printf_parse("done_word return 0: true null, ignored\n");
		return 0;
	}
	if (ctx->pending_redirect) {
		/* We do not glob in e.g. >*.tmp case. bash seems to glob here
		 * only if run as "bash", not "sh" */
		ctx->pending_redirect->rd_filename = xstrdup(word->data);
		word->o_assignment = NOT_ASSIGNMENT;
		debug_printf("word stored in rd_filename: '%s'\n", word->data);
	} else {
		if (child->group) { /* TODO: example how to trigger? */
			syntax(NULL);
			debug_printf_parse("done_word return 1: syntax error, groups and arglists don't mix\n");
			return 1;
		}
#if HAS_KEYWORDS
		if (!child->argv) { /* if it's the first word... */
			debug_printf_parse(": checking '%s' for reserved-ness\n", word->data);
			if (reserved_word(word, ctx)) {
				o_reset(word);
				word->o_assignment = NOT_ASSIGNMENT;
				debug_printf_parse("done_word return %d\n", (ctx->ctx_res_w == RES_SNTX));
				return (ctx->ctx_res_w == RES_SNTX);
			}
		}
#endif
		if (word->nonnull /* we saw "xx" or 'xx' */
		 /* optimization: and if it's ("" or '') or ($v... or `cmd`...): */
		 && (word->data[0] == '\0' || word->data[0] == SPECIAL_VAR_SYMBOL)
		 /* (otherwise it's "abc".... and is already safe) */
		) {
			/* Insert "empty variable" reference, this makes
			 * e.g. "", $empty"" etc to not disappear */
			o_addchr(word, SPECIAL_VAR_SYMBOL);
			o_addchr(word, SPECIAL_VAR_SYMBOL);
		}
		child->argv = add_malloced_string_to_strings(child->argv, xstrdup(word->data));
		debug_print_strings("word appended to argv", child->argv);
	}

	o_reset(word);
	ctx->pending_redirect = NULL;

#if ENABLE_HUSH_LOOPS
	/* Force FOR to have just one word (variable name) */
	if (ctx->ctx_res_w == RES_FOR)
		done_pipe(ctx, PIPE_SEQ);
#endif
	debug_printf_parse("done_word return 0\n");
	return 0;
}

/* Command (member of a pipe) is complete. The only possible error here
 * is out of memory, in which case xmalloc exits. */
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
	child->family = pi;

	ctx->child = child;
	/* but ctx->pipe and ctx->list_head remain unchanged */

	return pi->num_progs; /* used only for 0/nonzero check */
}

static void done_pipe(struct p_context *ctx, pipe_style type)
{
	int not_null;

	debug_printf_parse("done_pipe entered, followup %d\n", type);
	not_null = done_command(ctx);  /* implicit closure of previous command */
	ctx->pipe->followup = type;
	IF_HAS_KEYWORDS(ctx->pipe->res_word = ctx->ctx_res_w;)
	IF_HAS_KEYWORDS(ctx->pipe->pi_inverted = ctx->ctx_inverted;)
	IF_HAS_KEYWORDS(ctx->ctx_inverted = 0;)
	/* Without this check, even just <enter> on command line generates
	 * tree of three NOPs (!). Which is harmless but annoying.
	 * IOW: it is safe to do it unconditionally.
	 * RES_IN case is for "for a in; do ..." (empty IN set)
	 * to work. */
	if (not_null USE_HUSH_LOOPS(|| ctx->pipe->res_word == RES_IN)) {
		struct pipe *new_p = new_pipe();
		ctx->pipe->next = new_p;
		ctx->pipe = new_p;
		ctx->child = NULL; /* needed! */
		/* Create the memory for child, roughly:
		 * ctx->pipe->progs = new struct child_prog;
		 * ctx->pipe->progs[0].family = ctx->pipe;
		 * ctx->child = &ctx->pipe->progs[0];
		 */
		done_command(ctx);
	}
	debug_printf_parse("done_pipe return\n");
}

/* Peek ahead in the in_str to find out if we have a "&n" construct,
 * as in "2>&1", that represents duplicating a file descriptor.
 * Return either -2 (syntax error), -1 (no &), or the number found.
 */
static int redirect_dup_num(struct in_str *input)
{
	int ch, d = 0, ok = 0;
	ch = i_peek(input);
	if (ch != '&') return -1;

	i_getch(input);  /* get the & */
	ch = i_peek(input);
	if (ch == '-') {
		i_getch(input);
		return -3;  /* "-" represents "close me" */
	}
	while (isdigit(ch)) {
		d = d*10 + (ch-'0');
		ok = 1;
		i_getch(input);
		ch = i_peek(input);
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
		if (!isdigit(o->data[num])) {
			return -1;
		}
	}
	num = atoi(o->data);
	o_reset(o);
	return num;
}

#if ENABLE_HUSH_TICK
static FILE *generate_stream_from_list(struct pipe *head)
{
	FILE *pf;
	int pid, channel[2];

	xpipe(channel);
/* *** NOMMU WARNING *** */
/* By using vfork here, we suspend parent till child exits or execs.
 * If child will not do it before it fills the pipe, it can block forever
 * in write(STDOUT_FILENO), and parent (shell) will be also stuck.
 * Try this script:
 * yes "0123456789012345678901234567890" | dd bs=32 count=64k >TESTFILE
 * huge=`cat TESTFILE` # will block here forever
 * echo OK
 */
	pid = BB_MMU ? fork() : vfork();
	if (pid < 0)
		bb_perror_msg_and_die(BB_MMU ? "fork" : "vfork");
	if (pid == 0) { /* child */
		if (ENABLE_HUSH_JOB)
			die_sleep = 0; /* let nofork's xfuncs die */
		close(channel[0]); /* NB: close _first_, then move fd! */
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
		/* Freeing 'head' here would break NOMMU. */
		_exit(run_list(head));
	}
	close(channel[1]);
	pf = fdopen(channel[0], "r");
	return pf;
	/* 'head' is freed by the caller */
}

/* Return code is exit status of the process that is run. */
static int process_command_subs(o_string *dest,
		struct in_str *input,
		const char *subst_end)
{
	int retcode, ch, eol_cnt;
	o_string result = NULL_O_STRING;
	struct p_context inner;
	FILE *p;
	struct in_str pipe_str;

	initialize_context(&inner);

	/* Recursion to generate command */
	retcode = parse_stream(&result, &inner, input, subst_end);
	if (retcode != 0)
		return retcode;  /* syntax error or EOF */
	done_word(&result, &inner);
	done_pipe(&inner, PIPE_SEQ);
	o_free(&result);

	p = generate_stream_from_list(inner.list_head);
	if (p == NULL)
		return 1;
	close_on_exec_on(fileno(p));
	setup_file_in_str(&pipe_str, p);

	/* Now send results of command back into original context */
	eol_cnt = 0;
	while ((ch = i_getch(&pipe_str)) != EOF) {
		if (ch == '\n') {
			eol_cnt++;
			continue;
		}
		while (eol_cnt) {
			o_addchr(dest, '\n');
			eol_cnt--;
		}
//		/* Even unquoted `echo '\'` results in two backslashes
//		 * (which are converted into one by globbing later) */
//		if (!dest->o_quote && ch == '\\') {
//			o_addchr(dest, ch);
//		}
		o_addQchr(dest, ch);
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
	if (rcode == 0) {
		done_word(dest, &sub); /* finish off the final word in the subcontext */
		done_pipe(&sub, PIPE_SEQ);  /* and the final command there, too */
		child->group = sub.list_head;
	}
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

#if ENABLE_HUSH_TICK
/* Subroutines for copying $(...) and `...` things */
static void add_till_backquote(o_string *dest, struct in_str *input);
/* '...' */
static void add_till_single_quote(o_string *dest, struct in_str *input)
{
	while (1) {
		int ch = i_getch(input);
		if (ch == EOF)
			break;
		if (ch == '\'')
			break;
		o_addchr(dest, ch);
	}
}
/* "...\"...`..`...." - do we need to handle "...$(..)..." too? */
static void add_till_double_quote(o_string *dest, struct in_str *input)
{
	while (1) {
		int ch = i_getch(input);
		if (ch == '"')
			break;
		if (ch == '\\') {  /* \x. Copy both chars. */
			o_addchr(dest, ch);
			ch = i_getch(input);
		}
		if (ch == EOF)
			break;
		o_addchr(dest, ch);
		if (ch == '`') {
			add_till_backquote(dest, input);
			o_addchr(dest, ch);
			continue;
		}
		//if (ch == '$') ...
	}
}
/* Process `cmd` - copy contents until "`" is seen. Complicated by
 * \` quoting.
 * "Within the backquoted style of command substitution, backslash
 * shall retain its literal meaning, except when followed by: '$', '`', or '\'.
 * The search for the matching backquote shall be satisfied by the first
 * backquote found without a preceding backslash; during this search,
 * if a non-escaped backquote is encountered within a shell comment,
 * a here-document, an embedded command substitution of the $(command)
 * form, or a quoted string, undefined results occur. A single-quoted
 * or double-quoted string that begins, but does not end, within the
 * "`...`" sequence produces undefined results."
 * Example                               Output
 * echo `echo '\'TEST\`echo ZZ\`BEST`    \TESTZZBEST
 */
static void add_till_backquote(o_string *dest, struct in_str *input)
{
	while (1) {
		int ch = i_getch(input);
		if (ch == '`')
			break;
		if (ch == '\\') {  /* \x. Copy both chars unless it is \` */
			int ch2 = i_getch(input);
			if (ch2 != '`' && ch2 != '$' && ch2 != '\\')
				o_addchr(dest, ch);
			ch = ch2;
		}
		if (ch == EOF)
			break;
		o_addchr(dest, ch);
	}
}
/* Process $(cmd) - copy contents until ")" is seen. Complicated by
 * quoting and nested ()s.
 * "With the $(command) style of command substitution, all characters
 * following the open parenthesis to the matching closing parenthesis
 * constitute the command. Any valid shell script can be used for command,
 * except a script consisting solely of redirections which produces
 * unspecified results."
 * Example                              Output
 * echo $(echo '(TEST)' BEST)           (TEST) BEST
 * echo $(echo 'TEST)' BEST)            TEST) BEST
 * echo $(echo \(\(TEST\) BEST)         ((TEST) BEST
 */
static void add_till_closing_curly_brace(o_string *dest, struct in_str *input)
{
	int count = 0;
	while (1) {
		int ch = i_getch(input);
		if (ch == EOF)
			break;
		if (ch == '(')
			count++;
		if (ch == ')')
			if (--count < 0)
				break;
		o_addchr(dest, ch);
		if (ch == '\'') {
			add_till_single_quote(dest, input);
			o_addchr(dest, ch);
			continue;
		}
		if (ch == '"') {
			add_till_double_quote(dest, input);
			o_addchr(dest, ch);
			continue;
		}
		if (ch == '\\') { /* \x. Copy verbatim. Important for  \(, \) */
			ch = i_getch(input);
			if (ch == EOF)
				break;
			o_addchr(dest, ch);
			continue;
		}
	}
}
#endif /* ENABLE_HUSH_TICK */

/* Return code: 0 for OK, 1 for syntax error */
static int handle_dollar(o_string *dest, struct in_str *input)
{
	int ch = i_peek(input);  /* first character after the $ */
	unsigned char quote_mask = dest->o_quote ? 0x80 : 0;

	debug_printf_parse("handle_dollar entered: ch='%c'\n", ch);
	if (isalpha(ch)) {
		o_addchr(dest, SPECIAL_VAR_SYMBOL);
		while (1) {
			debug_printf_parse(": '%c'\n", ch);
			i_getch(input);
			o_addchr(dest, ch | quote_mask);
			quote_mask = 0;
			ch = i_peek(input);
			if (!isalnum(ch) && ch != '_')
				break;
		}
		o_addchr(dest, SPECIAL_VAR_SYMBOL);
	} else if (isdigit(ch)) {
 make_one_char_var:
		o_addchr(dest, SPECIAL_VAR_SYMBOL);
		debug_printf_parse(": '%c'\n", ch);
		i_getch(input);
		o_addchr(dest, ch | quote_mask);
		o_addchr(dest, SPECIAL_VAR_SYMBOL);
	} else switch (ch) {
		case '$': /* pid */
		case '!': /* last bg pid */
		case '?': /* last exit code */
		case '#': /* number of args */
		case '*': /* args */
		case '@': /* args */
			goto make_one_char_var;
		case '{':
			o_addchr(dest, SPECIAL_VAR_SYMBOL);
			i_getch(input);
			/* XXX maybe someone will try to escape the '}' */
			while (1) {
				ch = i_getch(input);
				if (ch == '}')
					break;
				if (!isalnum(ch) && ch != '_') {
					syntax("unterminated ${name}");
					debug_printf_parse("handle_dollar return 1: unterminated ${name}\n");
					return 1;
				}
				debug_printf_parse(": '%c'\n", ch);
				o_addchr(dest, ch | quote_mask);
				quote_mask = 0;
			}
			o_addchr(dest, SPECIAL_VAR_SYMBOL);
			break;
#if ENABLE_HUSH_TICK
		case '(': {
			//int pos = dest->length;
			i_getch(input);
			o_addchr(dest, SPECIAL_VAR_SYMBOL);
			o_addchr(dest, quote_mask | '`');
			add_till_closing_curly_brace(dest, input);
			//debug_printf_subst("SUBST RES2 '%s'\n", dest->data + pos);
			o_addchr(dest, SPECIAL_VAR_SYMBOL);
			break;
		}
#endif
		case '-':
		case '_':
			/* still unhandled, but should be eventually */
			bb_error_msg("unhandled syntax: $%c", ch);
			return 1;
			break;
		default:
			o_addQchr(dest, '$');
	}
	debug_printf_parse("handle_dollar return 0\n");
	return 0;
}

/* Scan input, call done_word() whenever full IFS delemited word was seen.
 * call done_pipe if '\n' was seen (and end_trigger != NULL)
 * Return if (non-quoted) char in end_trigger was seen; or on parse error. */
/* Return code is 0 if end_trigger char is met,
 * -1 on EOF (but if end_trigger == NULL then return 0)
 * 1 for syntax error */
static int parse_stream(o_string *dest, struct p_context *ctx,
	struct in_str *input, const char *end_trigger)
{
	int ch, m;
	int redir_fd;
	redir_type redir_style;
	int shadow_quote = dest->o_quote;
	int next;

	/* Only double-quote state is handled in the state variable dest->o_quote.
	 * A single-quote triggers a bypass of the main loop until its mate is
	 * found.  When recursing, quote state is passed in via dest->o_quote. */

	debug_printf_parse("parse_stream entered, end_trigger='%s'\n", end_trigger);

	while (1) {
		m = CHAR_IFS;
		next = '\0';
		ch = i_getch(input);
		if (ch != EOF) {
			m = charmap[ch];
			if (ch != '\n') {
				next = i_peek(input);
			}
		}
		debug_printf_parse(": ch=%c (%d) m=%d quote=%d\n",
						ch, ch, m, dest->o_quote);
		if (m == CHAR_ORDINARY
		 || (m != CHAR_SPECIAL && shadow_quote)
		) {
			if (ch == EOF) {
				syntax("unterminated \"");
				debug_printf_parse("parse_stream return 1: unterminated \"\n");
				return 1;
			}
			o_addQchr(dest, ch);
			if (dest->o_assignment == MAYBE_ASSIGNMENT
			 && ch == '='
			 && is_assignment(dest->data)
			) {
				dest->o_assignment = DEFINITELY_ASSIGNMENT;
			}
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
		if (end_trigger) {
			if (!shadow_quote && strchr(end_trigger, ch)) {
				/* Special case: (...word) makes last word terminate,
				 * as if ';' is seen */
				if (ch == ')') {
					done_word(dest, ctx);
//err chk?
					done_pipe(ctx, PIPE_SEQ);
				}
				if (!HAS_KEYWORDS IF_HAS_KEYWORDS(|| ctx->ctx_res_w == RES_NONE)) {
					debug_printf_parse("parse_stream return 0: end_trigger char found\n");
					return 0;
				}
			}
		}
		if (m == CHAR_IFS)
			continue;

		if (dest->o_assignment == MAYBE_ASSIGNMENT) {
			/* ch is a special char and thus this word
			 * cannot be an assignment: */
			dest->o_assignment = NOT_ASSIGNMENT;
		}

		switch (ch) {
		case '#':
			if (dest->length == 0 && !shadow_quote) {
				while (1) {
					ch = i_peek(input);
					if (ch == EOF || ch == '\n')
						break;
					i_getch(input);
				}
			} else {
				o_addQchr(dest, ch);
			}
			break;
		case '\\':
			if (next == EOF) {
				syntax("\\<eof>");
				debug_printf_parse("parse_stream return 1: \\<eof>\n");
				return 1;
			}
			/* bash:
			 * "The backslash retains its special meaning [in "..."]
			 * only when followed by one of the following characters:
			 * $, `, ", \, or <newline>.  A double quote may be quoted
			 * within double quotes by preceding it with a  backslash.
			 * If enabled, history expansion will be performed unless
			 * an ! appearing in double quotes is escaped using
			 * a backslash. The backslash preceding the ! is not removed."
			 */
			if (shadow_quote) { //NOT SURE   dest->o_quote) {
				if (strchr("$`\"\\", next) != NULL) {
					o_addqchr(dest, i_getch(input));
				} else {
					o_addqchr(dest, '\\');
				}
			} else {
				o_addchr(dest, '\\');
				o_addchr(dest, i_getch(input));
			}
			break;
		case '$':
			if (handle_dollar(dest, input) != 0) {
				debug_printf_parse("parse_stream return 1: handle_dollar returned non-0\n");
				return 1;
			}
			break;
		case '\'':
			dest->nonnull = 1;
			while (1) {
				ch = i_getch(input);
				if (ch == EOF) {
					syntax("unterminated '");
					debug_printf_parse("parse_stream return 1: unterminated '\n");
					return 1;
				}
				if (ch == '\'')
					break;
				if (dest->o_assignment == NOT_ASSIGNMENT)
					o_addqchr(dest, ch);
				else
					o_addchr(dest, ch);
			}
			break;
		case '"':
			dest->nonnull = 1;
			shadow_quote ^= 1; /* invert */
			if (dest->o_assignment == NOT_ASSIGNMENT)
				dest->o_quote ^= 1;
			break;
#if ENABLE_HUSH_TICK
		case '`': {
			//int pos = dest->length;
			o_addchr(dest, SPECIAL_VAR_SYMBOL);
			o_addchr(dest, shadow_quote /*or dest->o_quote??*/ ? 0x80 | '`' : '`');
			add_till_backquote(dest, input);
			o_addchr(dest, SPECIAL_VAR_SYMBOL);
			//debug_printf_subst("SUBST RES3 '%s'\n", dest->data + pos);
			break;
		}
#endif
		case '>':
			redir_fd = redirect_opt_num(dest);
			done_word(dest, ctx);
			redir_style = REDIRECT_OVERWRITE;
			if (next == '>') {
				redir_style = REDIRECT_APPEND;
				i_getch(input);
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
				i_getch(input);
			} else if (next == '>') {
				redir_style = REDIRECT_IO;
				i_getch(input);
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
				i_getch(input);
				done_pipe(ctx, PIPE_AND);
			} else {
				done_pipe(ctx, PIPE_BG);
			}
			break;
		case '|':
			done_word(dest, ctx);
			if (next == '|') {
				i_getch(input);
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
			/* proper use of this character is caught by end_trigger */
			syntax("unexpected } or )");
			debug_printf_parse("parse_stream return 1: unexpected '}'\n");
			return 1;
		default:
			if (HUSH_DEBUG)
				bb_error_msg_and_die("BUG: unexpected %c\n", ch);
		}
	} /* while (1) */
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

/* Most recursion does not come through here, the exception is
 * from builtin_source() and builtin_eval() */
static int parse_and_run_stream(struct in_str *inp, int parse_flag)
{
	struct p_context ctx;
	o_string temp = NULL_O_STRING;
	int rcode;

	do {
		initialize_context(&ctx);
		update_charmap();
#if ENABLE_HUSH_INTERACTIVE
		inp->promptmode = 0; /* PS1 */
#endif
		/* We will stop & execute after each ';' or '\n'.
		 * Example: "sleep 9999; echo TEST" + ctrl-C:
		 * TEST should be printed */
		rcode = parse_stream(&temp, &ctx, inp, ";\n");
#if HAS_KEYWORDS
		if (rcode != 1 && ctx.old_flag != 0) {
			syntax(NULL);
		}
#endif
		if (rcode != 1 IF_HAS_KEYWORDS(&& ctx.old_flag == 0)) {
			done_word(&temp, &ctx);
			done_pipe(&ctx, PIPE_SEQ);
			debug_print_tree(ctx.list_head, 0);
			debug_printf_exec("parse_stream_outer: run_and_free_list\n");
			run_and_free_list(ctx.list_head);
		} else {
			/* We arrive here also if rcode == 1 (error in parse_stream) */
#if HAS_KEYWORDS
			if (ctx.old_flag != 0) {
				free(ctx.stack);
				o_reset(&temp);
			}
#endif
			/*temp.nonnull = 0; - o_free does it below */
			/*temp.o_quote = 0; - o_free does it below */
			free_pipe_list(ctx.list_head, /* indent: */ 0);
			/* Discard all unprocessed line input, force prompt on */
			inp->p = NULL;
#if ENABLE_HUSH_INTERACTIVE
			inp->promptme = 1;
#endif
		}
		o_free(&temp);
		/* loop on syntax errors, return on EOF: */
	} while (rcode != -1 && !(parse_flag & PARSEFLAG_EXIT_FROM_LOOP));
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
	rcode = parse_and_run_stream(&input, 0 /* parse_flag */);
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

	INIT_G();

	root_pid = getpid();

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
			opt = parse_and_run_string(optarg, 0 /* parse_flag */);
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
		fcntl(interactive_fd, F_SETFD, FD_CLOEXEC);
		/* Looks like they want an interactive shell */
		setup_job_control();
		/* -1 is special - makes xfuncs longjmp, not exit
		 * (we reset die_sleep = 0 whereever we [v]fork) */
		die_sleep = -1;
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
		if (interactive_fd) {
			fcntl(interactive_fd, F_SETFD, FD_CLOEXEC);
			set_misc_sighandler(SIG_IGN);
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


/*
 * Built-ins
 */
static int builtin_true(char **argv ATTRIBUTE_UNUSED)
{
	return 0;
}

static int builtin_test(char **argv)
{
	int argc = 0;
	while (*argv) {
		argc++;
		argv++;
	}
	return test_main(argc, argv - argc);
}

static int builtin_echo(char **argv)
{
	int argc = 0;
	while (*argv) {
		argc++;
		argv++;
	}
	return echo_main(argc, argv - argc);
}

static int builtin_eval(char **argv)
{
	int rcode = EXIT_SUCCESS;

	if (argv[1]) {
		char *str = expand_strvec_to_string(argv + 1);
		parse_and_run_string(str, PARSEFLAG_EXIT_FROM_LOOP);
		free(str);
		rcode = last_return_code;
	}
	return rcode;
}

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

static int builtin_exec(char **argv)
{
	if (argv[1] == NULL)
		return EXIT_SUCCESS; /* bash does this */
	{
#if !BB_MMU
		char **ptrs2free = alloc_ptrs(argv);
#endif
// FIXME: if exec fails, bash does NOT exit! We do...
		pseudo_exec_argv(ptrs2free, argv + 1);
		/* never returns */
	}
}

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

#if ENABLE_HUSH_HELP
static int builtin_help(char **argv ATTRIBUTE_UNUSED)
{
	const struct built_in_command *x;

	printf("\nBuilt-in commands:\n");
	printf("-------------------\n");
	for (x = bltins; x != &bltins[ARRAY_SIZE(bltins)]; x++) {
		printf("%s\t%s\n", x->cmd, x->descr);
	}
	printf("\n\n");
	return EXIT_SUCCESS;
}
#endif

#if ENABLE_HUSH_JOB
static int builtin_jobs(char **argv ATTRIBUTE_UNUSED)
{
	struct pipe *job;
	const char *status_string;

	for (job = job_list; job; job = job->next) {
		if (job->alive_progs == job->stopped_progs)
			status_string = "Stopped";
		else
			status_string = "Running";

		printf(JOB_STATUS_FORMAT, job->jobid, status_string, job->cmdtext);
	}
	return EXIT_SUCCESS;
}
#endif

static int builtin_pwd(char **argv ATTRIBUTE_UNUSED)
{
	puts(set_cwd());
	return EXIT_SUCCESS;
}

static int builtin_read(char **argv)
{
	char *string;
	const char *name = argv[1] ? argv[1] : "REPLY";

	string = xmalloc_reads(STDIN_FILENO, xasprintf("%s=", name), NULL);
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

static int builtin_unset(char **argv)
{
	/* bash always returns true */
	unset_local_var(argv[1]);
	return EXIT_SUCCESS;
}
