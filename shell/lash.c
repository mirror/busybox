/* vi: set sw=4 ts=4: */
/*
 * lash -- the BusyBox Lame-Ass SHell
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Based in part on ladsh.c by Michael K. Johnson and Erik W. Troan, which is
 * under the following liberal license: "We have placed this source code in the
 * public domain. Use it in any project, free or commercial."
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

/* This shell's parsing engine is officially at a dead-end.  Future
 * work shell work should be done using hush, msh, or ash.  This is
 * still a very useful, small shell -- it just don't need any more
 * features beyond what it already has...
 */

//For debugging/development on the shell only...
//#define DEBUG_SHELL


#include "busybox.h"
#include <getopt.h>
#include "cmdedit.h"

#include <glob.h>
#define expand_t	glob_t

/* Always enable for the moment... */
#define CONFIG_LASH_PIPE_N_REDIRECTS
#define CONFIG_LASH_JOB_CONTROL

static const int MAX_READ = 128;	/* size of input buffer for `read' builtin */
#define JOB_STATUS_FORMAT "[%d] %-22s %.40s\n"


#ifdef CONFIG_LASH_PIPE_N_REDIRECTS
enum redir_type { REDIRECT_INPUT, REDIRECT_OVERWRITE,
	REDIRECT_APPEND
};
#endif

enum {
	DEFAULT_CONTEXT = 0x1,
	IF_TRUE_CONTEXT = 0x2,
	IF_FALSE_CONTEXT = 0x4,
	THEN_EXP_CONTEXT = 0x8,
	ELSE_EXP_CONTEXT = 0x10
};

#define LASH_OPT_DONE (1)
#define LASH_OPT_SAW_QUOTE (2)

#ifdef CONFIG_LASH_PIPE_N_REDIRECTS
struct redir_struct {
	enum redir_type type;	/* type of redirection */
	int fd;						/* file descriptor being redirected */
	char *filename;				/* file to redirect fd to */
};
#endif

struct child_prog {
	pid_t pid;					/* 0 if exited */
	char **argv;				/* program name and arguments */
	int num_redirects;			/* elements in redirection array */
	int is_stopped;				/* is the program currently running? */
	struct job *family;			/* pointer back to the child's parent job */
#ifdef CONFIG_LASH_PIPE_N_REDIRECTS
	struct redir_struct *redirects;	/* I/O redirects */
#endif
};

struct jobset {
	struct job *head;			/* head of list of running jobs */
	struct job *fg;				/* current foreground job */
};

struct job {
	int jobid;					/* job number */
	int num_progs;				/* total number of programs in job */
	int running_progs;			/* number of programs running */
	char *text;					/* name of job */
	char *cmdbuf;				/* buffer various argv's point into */
	pid_t pgrp;					/* process group ID for the job */
	struct child_prog *progs;	/* array of programs in job */
	struct job *next;			/* to track background commands */
	int stopped_progs;			/* number of programs alive, but stopped */
	unsigned int job_context;	/* bitmask defining current context */
	struct jobset *job_list;
};

struct built_in_command {
	char *cmd;					/* name */
	char *descr;				/* description */
	int (*function) (struct child_prog *);	/* function ptr */
};

/* function prototypes for builtins */
static int builtin_cd(struct child_prog *cmd);
static int builtin_exec(struct child_prog *cmd);
static int builtin_exit(struct child_prog *cmd);
static int builtin_fg_bg(struct child_prog *cmd);
static int builtin_help(struct child_prog *cmd);
static int builtin_jobs(struct child_prog *dummy);
static int builtin_pwd(struct child_prog *dummy);
static int builtin_export(struct child_prog *cmd);
static int builtin_source(struct child_prog *cmd);
static int builtin_unset(struct child_prog *cmd);
static int builtin_read(struct child_prog *cmd);


/* function prototypes for shell stuff */
static void checkjobs(struct jobset *job_list);
static void remove_job(struct jobset *j_list, struct job *job);
static int get_command(FILE * source, char *command);
static int parse_command(char **command_ptr, struct job *job, int *inbg);
static int run_command(struct job *newjob, int inbg, int outpipe[2]);
static int pseudo_exec(struct child_prog *cmd) ATTRIBUTE_NORETURN;
static int busy_loop(FILE * input);


/* Table of built-in functions (these are non-forking builtins, meaning they
 * can change global variables in the parent shell process but they will not
 * work with pipes and redirects; 'unset foo | whatever' will not work) */
static struct built_in_command bltins[] = {
	{"bg", "Resume a job in the background", builtin_fg_bg},
	{"cd", "Change working directory", builtin_cd},
	{"exec", "Exec command, replacing this shell with the exec'd process", builtin_exec},
	{"exit", "Exit from shell()", builtin_exit},
	{"fg", "Bring job into the foreground", builtin_fg_bg},
	{"jobs", "Lists the active jobs", builtin_jobs},
	{"export", "Set environment variable", builtin_export},
	{"unset", "Unset environment variable", builtin_unset},
	{"read", "Input environment variable", builtin_read},
	{".", "Source-in and run commands in a file", builtin_source},
	/* to do: add ulimit */
	{NULL, NULL, NULL}
};

/* Table of forking built-in functions (things that fork cannot change global
 * variables in the parent process, such as the current working directory) */
static struct built_in_command bltins_forking[] = {
	{"pwd", "Print current directory", builtin_pwd},
	{"help", "List shell built-in commands", builtin_help},
	{NULL, NULL, NULL}
};


static int shell_context;  /* Type prompt trigger (PS1 or PS2) */


/* Globals that are static to this file */
static const char *cwd;
static char *local_pending_command;
static struct jobset job_list = { NULL, NULL };
static int argc;
static char **argv;
static llist_t *close_me_list;
static int last_return_code;
static int last_bg_pid;
static unsigned int last_jobid;
static int shell_terminal;
static char *PS1;
static char *PS2 = "> ";


#ifdef DEBUG_SHELL
static inline void debug_printf(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}
#else
static inline void debug_printf(const char ATTRIBUTE_UNUSED *format, ...) { }
#endif

/*
	Most builtins need access to the struct child_prog that has
	their arguments, previously coded as cmd->progs[0].  That coding
	can exhibit a bug, if the builtin is not the first command in
	a pipeline: "echo foo | exec sort" will attempt to exec foo.

builtin   previous use      notes
------ -----------------  ---------
cd      cmd->progs[0]
exec    cmd->progs[0]  squashed bug: didn't look for applets or forking builtins
exit    cmd->progs[0]
fg_bg   cmd->progs[0], job_list->head, job_list->fg
help    0
jobs    job_list->head
pwd     0
export  cmd->progs[0]
source  cmd->progs[0]
unset   cmd->progs[0]
read    cmd->progs[0]

I added "struct job *family;" to struct child_prog,
and switched API to builtin_foo(struct child_prog *child);
So   cmd->text        becomes  child->family->text
     cmd->job_context  becomes  child->family->job_context
     cmd->progs[0]    becomes  *child
     job_list          becomes  child->family->job_list
 */

/* built-in 'cd <path>' handler */
static int builtin_cd(struct child_prog *child)
{
	char *newdir;

	if (child->argv[1] == NULL)
		newdir = getenv("HOME");
	else
		newdir = child->argv[1];
	if (chdir(newdir)) {
		bb_perror_msg("cd: %s", newdir);
		return EXIT_FAILURE;
	}
	cwd = xgetcwd((char *)cwd);
	if (!cwd)
		cwd = bb_msg_unknown;
	return EXIT_SUCCESS;
}

/* built-in 'exec' handler */
static int builtin_exec(struct child_prog *child)
{
	if (child->argv[1] == NULL)
		return EXIT_SUCCESS;   /* Really? */
	child->argv++;
	while(close_me_list) close((long)llist_pop(&close_me_list));
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

/* built-in 'fg' and 'bg' handler */
static int builtin_fg_bg(struct child_prog *child)
{
	int i, jobnum;
	struct job *job=NULL;

	/* If they gave us no args, assume they want the last backgrounded task */
	if (!child->argv[1]) {
		for (job = child->family->job_list->head; job; job = job->next) {
			if (job->jobid == last_jobid) {
				break;
			}
		}
		if (!job) {
			bb_error_msg("%s: no current job", child->argv[0]);
			return EXIT_FAILURE;
		}
	} else {
		if (sscanf(child->argv[1], "%%%d", &jobnum) != 1) {
			bb_error_msg(bb_msg_invalid_arg, child->argv[1], child->argv[0]);
			return EXIT_FAILURE;
		}
		for (job = child->family->job_list->head; job; job = job->next) {
			if (job->jobid == jobnum) {
				break;
			}
		}
		if (!job) {
			bb_error_msg("%s: %d: no such job", child->argv[0], jobnum);
			return EXIT_FAILURE;
		}
	}

	if (*child->argv[0] == 'f') {
		/* Put the job into the foreground.  */
		tcsetpgrp(shell_terminal, job->pgrp);

		child->family->job_list->fg = job;
	}

	/* Restart the processes in the job */
	for (i = 0; i < job->num_progs; i++)
		job->progs[i].is_stopped = 0;

	job->stopped_progs = 0;

	if ( (i=kill(- job->pgrp, SIGCONT)) < 0) {
		if (i == ESRCH) {
			remove_job(&job_list, job);
		} else {
			bb_perror_msg("kill (SIGCONT)");
		}
	}

	return EXIT_SUCCESS;
}

/* built-in 'help' handler */
static int builtin_help(struct child_prog ATTRIBUTE_UNUSED *dummy)
{
	struct built_in_command *x;

	printf("\nBuilt-in commands:\n"
		   "-------------------\n");
	for (x = bltins; x->cmd; x++) {
		if (x->descr==NULL)
			continue;
		printf("%s\t%s\n", x->cmd, x->descr);
	}
	for (x = bltins_forking; x->cmd; x++) {
		if (x->descr==NULL)
			continue;
		printf("%s\t%s\n", x->cmd, x->descr);
	}
	putchar('\n');
	return EXIT_SUCCESS;
}

/* built-in 'jobs' handler */
static int builtin_jobs(struct child_prog *child)
{
	struct job *job;
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
static int builtin_pwd(struct child_prog ATTRIBUTE_UNUSED *dummy)
{
	cwd = xgetcwd((char *)cwd);
	if (!cwd)
		cwd = bb_msg_unknown;
	puts(cwd);
	return EXIT_SUCCESS;
}

/* built-in 'export VAR=value' handler */
static int builtin_export(struct child_prog *child)
{
	int res;
	char *v = child->argv[1];

	if (v == NULL) {
		char **e;
		for (e = environ; *e; e++) {
			puts(*e);
		}
		return 0;
	}
	res = putenv(v);
	if (res)
		bb_perror_msg("export");
#ifdef CONFIG_FEATURE_SH_FANCY_PROMPT
	if (strncmp(v, "PS1=", 4)==0)
		PS1 = getenv("PS1");
#endif

#ifdef CONFIG_LOCALE_SUPPORT
	// TODO: why getenv? "" would be just as good...
	if(strncmp(v, "LC_ALL=", 7)==0)
		setlocale(LC_ALL, getenv("LC_ALL"));
	if(strncmp(v, "LC_CTYPE=", 9)==0)
		setlocale(LC_CTYPE, getenv("LC_CTYPE"));
#endif

	return res;
}

/* built-in 'read VAR' handler */
static int builtin_read(struct child_prog *child)
{
	int res = 0, len;
	char *s;
	char string[MAX_READ];

	if (child->argv[1]) {
		/* argument (VAR) given: put "VAR=" into buffer */
		safe_strncpy(string, child->argv[1], MAX_READ-1);
		len = strlen(string);
		string[len++] = '=';
		string[len]   = '\0';
		fgets(&string[len], sizeof(string) - len, stdin);	/* read string */
		res = strlen(string);
		if (res > len)
			string[--res] = '\0';	/* chomp trailing newline */
		/*
		** string should now contain "VAR=<value>"
		** copy it (putenv() won't do that, so we must make sure
		** the string resides in a static buffer!)
		*/
		res = -1;
		if ((s = strdup(string)))
			res = putenv(s);
		if (res)
			bb_perror_msg("read");
	}
	else
		fgets(string, sizeof(string), stdin);

	return res;
}

/* Built-in '.' handler (read-in and execute commands from file) */
static int builtin_source(struct child_prog *child)
{
	FILE *input;
	int status;

	input = fopen_or_warn(child->argv[1], "r");
	if (!input) {
		return EXIT_FAILURE;
	}

	llist_add_to(&close_me_list, (void *)(long)fileno(input));
	/* Now run the file */
	status = busy_loop(input);
	fclose(input);
	llist_pop(&close_me_list);
	return status;
}

/* built-in 'unset VAR' handler */
static int builtin_unset(struct child_prog *child)
{
	if (child->argv[1] == NULL) {
		printf(bb_msg_requires_arg, "unset");
		return EXIT_FAILURE;
	}
	unsetenv(child->argv[1]);
	return EXIT_SUCCESS;
}

#ifdef CONFIG_LASH_JOB_CONTROL
/* free up all memory from a job */
static void free_job(struct job *cmd)
{
	int i;
	struct jobset *keep;

	for (i = 0; i < cmd->num_progs; i++) {
		free(cmd->progs[i].argv);
#ifdef CONFIG_LASH_PIPE_N_REDIRECTS
		if (cmd->progs[i].redirects)
			free(cmd->progs[i].redirects);
#endif
	}
	free(cmd->progs);
	free(cmd->text);
	free(cmd->cmdbuf);
	keep = cmd->job_list;
	memset(cmd, 0, sizeof(struct job));
	cmd->job_list = keep;
}

/* remove a job from a jobset */
static void remove_job(struct jobset *j_list, struct job *job)
{
	struct job *prevjob;

	free_job(job);
	if (job == j_list->head) {
		j_list->head = job->next;
	} else {
		prevjob = j_list->head;
		while (prevjob->next != job)
			prevjob = prevjob->next;
		prevjob->next = job->next;
	}

	if (j_list->head)
		last_jobid = j_list->head->jobid;
	else
		last_jobid = 0;

	free(job);
}

/* Checks to see if any background processes have exited -- if they
   have, figure out why and see if a job has completed */
static void checkjobs(struct jobset *j_list)
{
	struct job *job;
	pid_t childpid;
	int status;
	int prognum = 0;

	while ((childpid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
		for (job = j_list->head; job; job = job->next) {
			prognum = 0;
			while (prognum < job->num_progs &&
				   job->progs[prognum].pid != childpid) prognum++;
			if (prognum < job->num_progs)
				break;
		}

		/* This happens on backticked commands */
		if(job==NULL)
			return;

		if (WIFEXITED(status) || WIFSIGNALED(status)) {
			/* child exited */
			job->running_progs--;
			job->progs[prognum].pid = 0;

			if (!job->running_progs) {
				printf(JOB_STATUS_FORMAT, job->jobid, "Done", job->text);
				last_jobid=0;
				remove_job(j_list, job);
			}
		} else {
			/* child stopped */
			job->stopped_progs++;
			job->progs[prognum].is_stopped = 1;
		}
	}

	if (childpid == -1 && errno != ECHILD)
		bb_perror_msg("waitpid");
}
#else
static void checkjobs(struct jobset *j_list)
{
}
static void free_job(struct job *cmd)
{
}
static void remove_job(struct jobset *j_list, struct job *job)
{
}
#endif

#ifdef CONFIG_LASH_PIPE_N_REDIRECTS
/* squirrel != NULL means we squirrel away copies of stdin, stdout,
 * and stderr if they are redirected. */
static int setup_redirects(struct child_prog *prog, int squirrel[])
{
	int i;
	int openfd;
	int mode = O_RDONLY;
	struct redir_struct *redir = prog->redirects;

	for (i = 0; i < prog->num_redirects; i++, redir++) {
		switch (redir->type) {
		case REDIRECT_INPUT:
			mode = O_RDONLY;
			break;
		case REDIRECT_OVERWRITE:
			mode = O_WRONLY | O_CREAT | O_TRUNC;
			break;
		case REDIRECT_APPEND:
			mode = O_WRONLY | O_CREAT | O_APPEND;
			break;
		}

		openfd = open(redir->filename, mode, 0666);
		if (openfd < 0) {
			/* this could get lost if stderr has been redirected, but
			   bash and ash both lose it as well (though zsh doesn't!) */
			bb_perror_msg("error opening %s", redir->filename);
			return 1;
		}

		if (openfd != redir->fd) {
			if (squirrel && redir->fd < 3) {
				squirrel[redir->fd] = dup(redir->fd);
				fcntl (squirrel[redir->fd], F_SETFD, FD_CLOEXEC);
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
#else
static inline int setup_redirects(struct child_prog *prog, int squirrel[])
{
	return 0;
}
static inline void restore_redirects(int squirrel[])
{
}
#endif

static inline void cmdedit_set_initial_prompt(void)
{
#ifndef CONFIG_FEATURE_SH_FANCY_PROMPT
	PS1 = NULL;
#else
	PS1 = getenv("PS1");
	if(PS1==0)
		PS1 = "\\w \\$ ";
#endif
}

static inline void setup_prompt_string(char **prompt_str)
{
#ifndef CONFIG_FEATURE_SH_FANCY_PROMPT
	/* Set up the prompt */
	if (shell_context == 0) {
		free(PS1);
		PS1=xmalloc(strlen(cwd)+4);
		sprintf(PS1, "%s %c ", cwd, ( geteuid() != 0 ) ? '$': '#');
		*prompt_str = PS1;
	} else {
		*prompt_str = PS2;
	}
#else
	*prompt_str = (shell_context==0)? PS1 : PS2;
#endif
}

static int get_command(FILE * source, char *command)
{
	char *prompt_str;

	if (source == NULL) {
		if (local_pending_command) {
			/* a command specified (-c option): return it & mark it done */
			strcpy(command, local_pending_command);
			free(local_pending_command);
			local_pending_command = NULL;
			return 0;
		}
		return 1;
	}

	if (source == stdin) {
		setup_prompt_string(&prompt_str);

#ifdef CONFIG_FEATURE_COMMAND_EDITING
		/*
		** enable command line editing only while a command line
		** is actually being read; otherwise, we'll end up bequeathing
		** atexit() handlers and other unwanted stuff to our
		** child processes (rob@sysgo.de)
		*/
		cmdedit_read_input(prompt_str, command);
		return 0;
#else
		fputs(prompt_str, stdout);
#endif
	}

	if (!fgets(command, BUFSIZ - 2, source)) {
		if (source == stdin)
			puts("");
		return 1;
	}

	return 0;
}

static char * strsep_space( char *string, int * ix)
{
	/* Short circuit the trivial case */
	if ( !string || ! string[*ix])
		return NULL;

	/* Find the end of the token. */
	while (string[*ix] && !isspace(string[*ix]) ) {
		(*ix)++;
	}

	/* Find the end of any whitespace trailing behind
	 * the token and let that be part of the token */
	while (string[*ix] && (isspace)(string[*ix]) ) {
		(*ix)++;
	}

	if (!*ix) {
		/* Nothing useful was found */
		return NULL;
	}

	return xstrndup(string, *ix);
}

static int expand_arguments(char *command)
{
	int total_length=0, length, i, retval, ix = 0;
	expand_t expand_result;
	char *tmpcmd, *cmd, *cmd_copy;
	char *src, *dst, *var;
	const char * const out_of_space = "out of space during expansion";
	int flags = GLOB_NOCHECK
#ifdef GLOB_BRACE
		| GLOB_BRACE
#endif
#ifdef GLOB_TILDE
		| GLOB_TILDE
#endif
		;

	/* get rid of the terminating \n */
	chomp(command);

	/* Fix up escape sequences to be the Real Thing(tm) */
	while( command && command[ix]) {
		if (command[ix] == '\\') {
			const char *tmp = command+ix+1;
			command[ix] = bb_process_escape_sequence(  &tmp );
			memmove(command+ix + 1, tmp, strlen(tmp)+1);
		}
		ix++;
	}
	/* Use glob and then fixup environment variables and such */

	/* It turns out that glob is very stupid.  We have to feed it one word at a
	 * time since it can't cope with a full string.  Here we convert command
	 * (char*) into cmd (char**, one word per string) */

	/* We need a clean copy, so strsep can mess up the copy while
	 * we write stuff into the original (in a minute) */
	cmd = cmd_copy = xstrdup(command);
	*command = '\0';
	for (ix = 0, tmpcmd = cmd;
			(tmpcmd = strsep_space(cmd, &ix)) != NULL; cmd += ix, ix=0) {
		if (*tmpcmd == '\0')
			break;
		/* we need to trim() the result for glob! */
		trim(tmpcmd);
		retval = glob(tmpcmd, flags, NULL, &expand_result);
		free(tmpcmd); /* Free mem allocated by strsep_space */
		if (retval == GLOB_NOSPACE) {
			/* Mem may have been allocated... */
			globfree (&expand_result);
			bb_error_msg(out_of_space);
			return FALSE;
		} else if (retval != 0) {
			/* Some other error.  GLOB_NOMATCH shouldn't
			 * happen because of the GLOB_NOCHECK flag in
			 * the glob call. */
			bb_error_msg("syntax error");
			return FALSE;
		} else {
			/* Convert from char** (one word per string) to a simple char*,
			 * but don't overflow command which is BUFSIZ in length */
			for (i=0; i < expand_result.gl_pathc; i++) {
				length=strlen(expand_result.gl_pathv[i]);
				if (total_length+length+1 >= BUFSIZ) {
					bb_error_msg(out_of_space);
					return FALSE;
				}
				strcat(command+total_length, " ");
				total_length+=1;
				strcat(command+total_length, expand_result.gl_pathv[i]);
				total_length+=length;
			}
			globfree (&expand_result);
		}
	}
	free(cmd_copy);
	trim(command);

	/* Now do the shell variable substitutions which
	 * wordexp can't do for us, namely $? and $! */
	src = command;
	while((dst = strchr(src,'$')) != NULL){
		var = NULL;
		switch (*(dst+1)) {
			case '?':
				var = itoa(last_return_code);
				break;
			case '!':
				if (last_bg_pid==-1)
					*(var)='\0';
				else
					var = itoa(last_bg_pid);
				break;
				/* Everything else like $$, $#, $[0-9], etc. should all be
				 * expanded by wordexp(), so we can in theory skip that stuff
				 * here, but just to be on the safe side (i.e., since uClibc
				 * wordexp doesn't do this stuff yet), lets leave it in for
				 * now. */
			case '$':
				var = itoa(getpid());
				break;
			case '#':
				var = itoa(argc-1);
				break;
			case '0':case '1':case '2':case '3':case '4':
			case '5':case '6':case '7':case '8':case '9':
				{
					int ixx=*(dst+1)-48+1;
					if (ixx >= argc) {
						var='\0';
					} else {
						var = argv[ixx];
					}
				}
				break;

		}
		if (var) {
			/* a single character construction was found, and
			 * already handled in the case statement */
			src=dst+2;
		} else {
			/* Looks like an environment variable */
			char delim_hold;
			int num_skip_chars=0;
			int dstlen = strlen(dst);
			/* Is this a ${foo} type variable? */
			if (dstlen >=2 && *(dst+1) == '{') {
				src=strchr(dst+1, '}');
				num_skip_chars=1;
			} else {
				src=dst+1;
				while((isalnum)(*src) || *src=='_') src++;
			}
			if (src == NULL) {
				src = dst+dstlen;
			}
			delim_hold=*src;
			*src='\0';  /* temporary */
			var = getenv(dst + 1 + num_skip_chars);
			*src=delim_hold;
			src += num_skip_chars;
		}
		if (var == NULL) {
			/* Seems we got an un-expandable variable.  So delete it. */
			var = "";
		}
		{
			int subst_len = strlen(var);
			int trail_len = strlen(src);
			if (dst+subst_len+trail_len >= command+BUFSIZ) {
				bb_error_msg(out_of_space);
				return FALSE;
			}
			/* Move stuff to the end of the string to accommodate
			 * filling the created gap with the new stuff */
			memmove(dst+subst_len, src, trail_len+1);
			/* Now copy in the new stuff */
			memcpy(dst, var, subst_len);
			src = dst+subst_len;
		}
	}

	return TRUE;
}

/* Return cmd->num_progs as 0 if no command is present (e.g. an empty
   line). If a valid command is found, command_ptr is set to point to
   the beginning of the next command (if the original command had more
   then one job associated with it) or NULL if no more commands are
   present. */
static int parse_command(char **command_ptr, struct job *job, int *inbg)
{
	char *command;
	char *return_command = NULL;
	char *src, *buf;
	int argc_l;
	int flag;
	int argv_alloced;
	char quote = '\0';
	struct child_prog *prog;
#ifdef CONFIG_LASH_PIPE_N_REDIRECTS
	int i;
	char *chptr;
#endif

	/* skip leading white space */
	*command_ptr = skip_whitespace(*command_ptr);

	/* this handles empty lines or leading '#' characters */
	if (!**command_ptr || (**command_ptr == '#')) {
		job->num_progs=0;
		return 0;
	}

	*inbg = 0;
	job->num_progs = 1;
	job->progs = xmalloc(sizeof(*job->progs));

	/* We set the argv elements to point inside of this string. The
	   memory is freed by free_job(). Allocate twice the original
	   length in case we need to quote every single character.

	   Getting clean memory relieves us of the task of NULL
	   terminating things and makes the rest of this look a bit
	   cleaner (though it is, admittedly, a tad less efficient) */
	job->cmdbuf = command = xzalloc(2*strlen(*command_ptr) + 1);
	job->text = NULL;

	prog = job->progs;
	prog->num_redirects = 0;
	prog->is_stopped = 0;
	prog->family = job;
#ifdef CONFIG_LASH_PIPE_N_REDIRECTS
	prog->redirects = NULL;
#endif

	argv_alloced = 5;
	prog->argv = xmalloc(sizeof(*prog->argv) * argv_alloced);
	prog->argv[0] = job->cmdbuf;

	flag = argc_l = 0;
	buf = command;
	src = *command_ptr;
	while (*src && !(flag & LASH_OPT_DONE)) {
		if (quote == *src) {
			quote = '\0';
		} else if (quote) {
			if (*src == '\\') {
				src++;
				if (!*src) {
					bb_error_msg("character expected after \\");
					free_job(job);
					return 1;
				}

				/* in shell, "\'" should yield \' */
				if (*src != quote) {
					*buf++ = '\\';
					*buf++ = '\\';
				}
			} else if (*src == '*' || *src == '?' || *src == '[' ||
					   *src == ']') *buf++ = '\\';
			*buf++ = *src;
		} else if (isspace(*src)) {
			if (*prog->argv[argc_l] || flag & LASH_OPT_SAW_QUOTE) {
				buf++, argc_l++;
				/* +1 here leaves room for the NULL which ends argv */
				if ((argc_l + 1) == argv_alloced) {
					argv_alloced += 5;
					prog->argv = xrealloc(prog->argv,
										  sizeof(*prog->argv) *
										  argv_alloced);
				}
				prog->argv[argc_l] = buf;
				flag ^= LASH_OPT_SAW_QUOTE;
			}
		} else
			switch (*src) {
			case '"':
			case '\'':
				quote = *src;
				flag |= LASH_OPT_SAW_QUOTE;
				break;

			case '#':			/* comment */
				if (*(src-1)== '$')
					*buf++ = *src;
				else
					flag |= LASH_OPT_DONE;
				break;

#ifdef CONFIG_LASH_PIPE_N_REDIRECTS
			case '>':			/* redirects */
			case '<':
				i = prog->num_redirects++;
				prog->redirects = xrealloc(prog->redirects,
											  sizeof(*prog->redirects) *
											  (i + 1));

				prog->redirects[i].fd = -1;
				if (buf != prog->argv[argc_l]) {
					/* the stuff before this character may be the file number
					   being redirected */
					prog->redirects[i].fd =
						strtol(prog->argv[argc_l], &chptr, 10);

					if (*chptr && *prog->argv[argc_l]) {
						buf++, argc_l++;
						prog->argv[argc_l] = buf;
					}
				}

				if (prog->redirects[i].fd == -1) {
					if (*src == '>')
						prog->redirects[i].fd = 1;
					else
						prog->redirects[i].fd = 0;
				}

				if (*src++ == '>') {
					if (*src == '>')
						prog->redirects[i].type =
							REDIRECT_APPEND, src++;
					else
						prog->redirects[i].type = REDIRECT_OVERWRITE;
				} else {
					prog->redirects[i].type = REDIRECT_INPUT;
				}

				/* This isn't POSIX sh compliant. Oh well. */
				chptr = src;
				chptr = skip_whitespace(chptr);

				if (!*chptr) {
					bb_error_msg("file name expected after %c", *(src-1));
					free_job(job);
					job->num_progs=0;
					return 1;
				}

				prog->redirects[i].filename = buf;
				while (*chptr && !isspace(*chptr))
					*buf++ = *chptr++;

				src = chptr - 1;	/* we src++ later */
				prog->argv[argc_l] = ++buf;
				break;

			case '|':			/* pipe */
				/* finish this command */
				if (*prog->argv[argc_l] || flag & LASH_OPT_SAW_QUOTE)
					argc_l++;
				if (!argc_l) {
					goto empty_command_in_pipe;
				}
				prog->argv[argc_l] = NULL;

				/* and start the next */
				job->num_progs++;
				job->progs = xrealloc(job->progs,
									  sizeof(*job->progs) * job->num_progs);
				prog = job->progs + (job->num_progs - 1);
				prog->num_redirects = 0;
				prog->redirects = NULL;
				prog->is_stopped = 0;
				prog->family = job;
				argc_l = 0;

				argv_alloced = 5;
				prog->argv = xmalloc(sizeof(*prog->argv) * argv_alloced);
				prog->argv[0] = ++buf;

				src++;
				src = skip_whitespace(src);

				if (!*src) {
empty_command_in_pipe:
					bb_error_msg("empty command in pipe");
					free_job(job);
					job->num_progs=0;
					return 1;
				}
				src--;			/* we'll ++ it at the end of the loop */

				break;
#endif

#ifdef CONFIG_LASH_JOB_CONTROL
			case '&':			/* background */
				*inbg = 1;
				/* fallthrough */
#endif
			case ';':			/* multiple commands */
				flag |= LASH_OPT_DONE;
				return_command = *command_ptr + (src - *command_ptr) + 1;
				break;

			case '\\':
				src++;
				if (!*src) {
					bb_error_msg("character expected after \\");
					free_job(job);
					return 1;
				}
				if (*src == '*' || *src == '[' || *src == ']'
					|| *src == '?') *buf++ = '\\';
				/* fallthrough */
			default:
				*buf++ = *src;
			}

		src++;
	}

	if (*prog->argv[argc_l] || flag & LASH_OPT_SAW_QUOTE) {
		argc_l++;
	}
	if (!argc_l) {
		free_job(job);
		return 0;
	}
	prog->argv[argc_l] = NULL;

	if (!return_command) {
		job->text = xstrdup(*command_ptr);
	} else {
		/* This leaves any trailing spaces, which is a bit sloppy */
		job->text = xstrndup(*command_ptr, return_command - *command_ptr);
	}

	*command_ptr = return_command;

	return 0;
}

/* Run the child_prog, no matter what kind of command it uses.
 */
static int pseudo_exec(struct child_prog *child)
{
	struct built_in_command *x;

	/* Check if the command matches any of the non-forking builtins.
	 * Depending on context, this might be redundant.  But it's
	 * easier to waste a few CPU cycles than it is to figure out
	 * if this is one of those cases.
	 */
	for (x = bltins; x->cmd; x++) {
		if (strcmp(child->argv[0], x->cmd) == 0 ) {
			_exit(x->function(child));
		}
	}

	/* Check if the command matches any of the forking builtins. */
	for (x = bltins_forking; x->cmd; x++) {
		if (strcmp(child->argv[0], x->cmd) == 0) {
			applet_name=x->cmd;
			_exit (x->function(child));
		}
	}

	/* Check if the command matches any busybox internal
	 * commands ("applets") here.  Following discussions from
	 * November 2000 on busybox@busybox.net, don't use
	 * bb_get_last_path_component().  This way explicit (with
	 * slashes) filenames will never be interpreted as an
	 * applet, just like with builtins.  This way the user can
	 * override an applet with an explicit filename reference.
	 * The only downside to this change is that an explicit
	 * /bin/foo invocation will fork and exec /bin/foo, even if
	 * /bin/foo is a symlink to busybox.
	 */

	if (ENABLE_FEATURE_SH_STANDALONE_SHELL) {
		char **argv_l = child->argv;
		int argc_l;

		for (argc_l=0; *argv_l; argv_l++, argc_l++);
		optind = 1;
		run_applet_by_name(child->argv[0], argc_l, child->argv);
	}

	execvp(child->argv[0], child->argv);

	/* Do not use bb_perror_msg_and_die() here, since we must not
	 * call exit() but should call _exit() instead */
	bb_perror_msg("%s", child->argv[0]);
	_exit(EXIT_FAILURE);
}

static void insert_job(struct job *newjob, int inbg)
{
	struct job *thejob;
	struct jobset *j_list=newjob->job_list;

	/* find the ID for thejob to use */
	newjob->jobid = 1;
	for (thejob = j_list->head; thejob; thejob = thejob->next)
		if (thejob->jobid >= newjob->jobid)
			newjob->jobid = thejob->jobid + 1;

	/* add thejob to the list of running jobs */
	if (!j_list->head) {
		thejob = j_list->head = xmalloc(sizeof(*thejob));
	} else {
		for (thejob = j_list->head; thejob->next; thejob = thejob->next) /* nothing */;
		thejob->next = xmalloc(sizeof(*thejob));
		thejob = thejob->next;
	}

	*thejob = *newjob;   /* physically copy the struct job */
	thejob->next = NULL;
	thejob->running_progs = thejob->num_progs;
	thejob->stopped_progs = 0;

#ifdef CONFIG_LASH_JOB_CONTROL
	if (inbg) {
		/* we don't wait for background thejobs to return -- append it
		   to the list of backgrounded thejobs and leave it alone */
		printf("[%d] %d\n", thejob->jobid,
			   newjob->progs[newjob->num_progs - 1].pid);
		last_jobid = newjob->jobid;
		last_bg_pid=newjob->progs[newjob->num_progs - 1].pid;
	} else {
		newjob->job_list->fg = thejob;

		/* move the new process group into the foreground */
		/* Ignore errors since child could have already exited */
		tcsetpgrp(shell_terminal, newjob->pgrp);
	}
#endif
}

static int run_command(struct job *newjob, int inbg, int outpipe[2])
{
	/* struct job *thejob; */
	int i;
	int nextin, nextout;
	int pipefds[2];				/* pipefd[0] is for reading */
	struct built_in_command *x;
	struct child_prog *child;

	nextin = 0, nextout = 1;
	for (i = 0; i < newjob->num_progs; i++) {
		child = & (newjob->progs[i]);

		if ((i + 1) < newjob->num_progs) {
			if (pipe(pipefds)<0) bb_perror_msg_and_die("pipe");
			nextout = pipefds[1];
		} else {
			if (outpipe[1]!=-1) {
				nextout = outpipe[1];
			} else {
				nextout = 1;
			}
		}


		/* Check if the command matches any non-forking builtins,
		 * but only if this is a simple command.
		 * Non-forking builtins within pipes have to fork anyway,
		 * and are handled in pseudo_exec.  "echo foo | read bar"
		 * is doomed to failure, and doesn't work on bash, either.
		 */
		if (newjob->num_progs == 1) {
			/* Check if the command sets an environment variable. */
			if (strchr(child->argv[0], '=') != NULL) {
				child->argv[1] = child->argv[0];
				return builtin_export(child);
			}

			for (x = bltins; x->cmd; x++) {
				if (strcmp(child->argv[0], x->cmd) == 0 ) {
					int rcode;
					int squirrel[] = {-1, -1, -1};
					setup_redirects(child, squirrel);
					rcode = x->function(child);
					restore_redirects(squirrel);
					return rcode;
				}
			}
		}

#if !defined(__UCLIBC__) || defined(__ARCH_HAS_MMU__)
		if (!(child->pid = fork()))
#else
		if (!(child->pid = vfork()))
#endif
		{
			/* Set the handling for job control signals back to the default.  */
			signal(SIGINT, SIG_DFL);
			signal(SIGQUIT, SIG_DFL);
			signal(SIGTSTP, SIG_DFL);
			signal(SIGTTIN, SIG_DFL);
			signal(SIGTTOU, SIG_DFL);
			signal(SIGCHLD, SIG_DFL);

			/* Close all open filehandles. */
			while(close_me_list) close((long)llist_pop(&close_me_list));

			if (outpipe[1]!=-1) {
				close(outpipe[0]);
			}
			if (nextin != 0) {
				dup2(nextin, 0);
				close(nextin);
			}

			if (nextout != 1) {
				dup2(nextout, 1);
				dup2(nextout, 2);  /* Really? */
				close(nextout);
				close(pipefds[0]);
			}

			/* explicit redirects override pipes */
			setup_redirects(child,NULL);

			pseudo_exec(child);
		}
		if (outpipe[1]!=-1) {
			close(outpipe[1]);
		}

		/* put our child in the process group whose leader is the
		   first process in this pipe */
		setpgid(child->pid, newjob->progs[0].pid);
		if (nextin != 0)
			close(nextin);
		if (nextout != 1)
			close(nextout);

		/* If there isn't another process, nextin is garbage
		   but it doesn't matter */
		nextin = pipefds[0];
	}

	newjob->pgrp = newjob->progs[0].pid;

	insert_job(newjob, inbg);

	return 0;
}

static int busy_loop(FILE * input)
{
	char *command;
	char *next_command = NULL;
	struct job newjob;
	int i;
	int inbg = 0;
	int status;
#ifdef CONFIG_LASH_JOB_CONTROL
	pid_t  parent_pgrp;
	/* save current owner of TTY so we can restore it on exit */
	parent_pgrp = tcgetpgrp(shell_terminal);
#endif
	newjob.job_list = &job_list;
	newjob.job_context = DEFAULT_CONTEXT;

	command = xzalloc(BUFSIZ);

	while (1) {
		if (!job_list.fg) {
			/* no job is in the foreground */

			/* see if any background processes have exited */
			checkjobs(&job_list);

			if (!next_command) {
				if (get_command(input, command))
					break;
				next_command = command;
			}

			if (! expand_arguments(next_command)) {
				free(command);
				command = xzalloc(BUFSIZ);
				next_command = NULL;
				continue;
			}

			if (!parse_command(&next_command, &newjob, &inbg) &&
				newjob.num_progs) {
				int pipefds[2] = {-1,-1};
				debug_printf( "job=%p fed to run_command by busy_loop()'\n",
						&newjob);
				run_command(&newjob, inbg, pipefds);
			}
			else {
				free(command);
				command = xzalloc(BUFSIZ);
				next_command = NULL;
			}
		} else {
			/* a job is running in the foreground; wait for it */
			i = 0;
			while (!job_list.fg->progs[i].pid ||
				   job_list.fg->progs[i].is_stopped == 1) i++;

			if (waitpid(job_list.fg->progs[i].pid, &status, WUNTRACED)<0) {
				if (errno != ECHILD) {
					bb_perror_msg_and_die("waitpid(%d)",job_list.fg->progs[i].pid);
				}
			}

			if (WIFEXITED(status) || WIFSIGNALED(status)) {
				/* the child exited */
				job_list.fg->running_progs--;
				job_list.fg->progs[i].pid = 0;

				last_return_code=WEXITSTATUS(status);

				if (!job_list.fg->running_progs) {
					/* child exited */
					remove_job(&job_list, job_list.fg);
					job_list.fg = NULL;
				}
			}
#ifdef CONFIG_LASH_JOB_CONTROL
			else {
				/* the child was stopped */
				job_list.fg->stopped_progs++;
				job_list.fg->progs[i].is_stopped = 1;

				if (job_list.fg->stopped_progs == job_list.fg->running_progs) {
					printf("\n" JOB_STATUS_FORMAT, job_list.fg->jobid,
						   "Stopped", job_list.fg->text);
					job_list.fg = NULL;
				}
			}

			if (!job_list.fg) {
				/* move the shell to the foreground */
				/* suppress messages when run from /linuxrc mag@sysgo.de */
				if (tcsetpgrp(shell_terminal, getpgrp()) && errno != ENOTTY)
					bb_perror_msg("tcsetpgrp");
			}
#endif
		}
	}
	free(command);

#ifdef CONFIG_LASH_JOB_CONTROL
	/* return controlling TTY back to parent process group before exiting */
	if (tcsetpgrp(shell_terminal, parent_pgrp) && errno != ENOTTY)
		bb_perror_msg("tcsetpgrp");
#endif

	/* return exit status if called with "-c" */
	if (input == NULL && WIFEXITED(status))
		return WEXITSTATUS(status);

	return 0;
}

#ifdef CONFIG_FEATURE_CLEAN_UP
static void free_memory(void)
{
	if (cwd && cwd!=bb_msg_unknown) {
		free((char*)cwd);
	}
	if (local_pending_command)
		free(local_pending_command);

	if (job_list.fg && !job_list.fg->running_progs) {
		remove_job(&job_list, job_list.fg);
	}
}
#else
void free_memory(void);
#endif

#ifdef CONFIG_LASH_JOB_CONTROL
/* Make sure we have a controlling tty.  If we get started under a job
 * aware app (like bash for example), make sure we are now in charge so
 * we don't fight over who gets the foreground */
static void setup_job_control(void)
{
	int status;
	pid_t shell_pgrp;

	/* Loop until we are in the foreground.  */
	while ((status = tcgetpgrp (shell_terminal)) >= 0) {
		if (status == (shell_pgrp = getpgrp ())) {
			break;
		}
		kill (- shell_pgrp, SIGTTIN);
	}

	/* Ignore interactive and job-control signals.  */
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGTTOU, SIG_IGN);
	signal(SIGCHLD, SIG_IGN);

	/* Put ourselves in our own process group.  */
	setsid();
	shell_pgrp = getpid();
	setpgid(shell_pgrp, shell_pgrp);

	/* Grab control of the terminal.  */
	tcsetpgrp(shell_terminal, shell_pgrp);
}
#else
static inline void setup_job_control(void)
{
}
#endif

int lash_main(int argc_l, char **argv_l)
{
	unsigned opt;
	FILE *input = stdin;
	argc = argc_l;
	argv = argv_l;

	/* These variables need re-initializing when recursing */
	last_jobid = 0;
	close_me_list = NULL;
	job_list.head = NULL;
	job_list.fg = NULL;
	last_return_code=1;

	if (argv[0] && argv[0][0] == '-') {
		FILE *prof_input;
		prof_input = fopen("/etc/profile", "r");
		if (prof_input) {
			llist_add_to(&close_me_list, (void *)(long)fileno(prof_input));
			/* Now run the file */
			busy_loop(prof_input);
			fclose_if_not_stdin(prof_input);
			llist_pop(&close_me_list);
		}
	}

	opt = getopt32(argc_l, argv_l, "+ic:", &local_pending_command);
#define LASH_OPT_i (1<<0)
#define LASH_OPT_c (1<<2)
	if (opt & LASH_OPT_c) {
		input = NULL;
		optind++;
		argv += optind;
	}
	/* A shell is interactive if the `-i' flag was given, or if all of
	 * the following conditions are met:
	 *	  no -c command
	 *    no arguments remaining or the -s flag given
	 *    standard input is a terminal
	 *    standard output is a terminal
	 *    Refer to Posix.2, the description of the `sh' utility. */
	if (argv[optind]==NULL && input==stdin &&
			isatty(STDIN_FILENO) && isatty(STDOUT_FILENO))
	{
		opt |= LASH_OPT_i;
	}
	setup_job_control();
	if (opt & LASH_OPT_i) {
		/* Looks like they want an interactive shell */
		if (!ENABLE_FEATURE_SH_EXTRA_QUIET) {
			printf("\n\n%s Built-in shell (lash)\n"
					"Enter 'help' for a list of built-in commands.\n\n",
					BB_BANNER);
		}
	} else if (!local_pending_command && argv[optind]) {
		//printf( "optind=%d  argv[optind]='%s'\n", optind, argv[optind]);
		input = xfopen(argv[optind], "r");
		/* be lazy, never mark this closed */
		llist_add_to(&close_me_list, (void *)(long)fileno(input));
	}

	/* initialize the cwd -- this is never freed...*/
	cwd = xgetcwd(0);
	if (!cwd)
		cwd = bb_msg_unknown;

	if (ENABLE_FEATURE_CLEAN_UP) atexit(free_memory);

	if (ENABLE_FEATURE_COMMAND_EDITING) cmdedit_set_initial_prompt();
	else PS1 = NULL;

	return (busy_loop(input));
}
