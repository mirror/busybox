/* vi: set sw=4 ts=4: */
/*
 * lash -- the BusyBox Lame-Ass SHell
 *
 * Copyright (C) 2000 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
 *
 * Based in part on ladsh.c by Michael K. Johnson and Erik W. Troan, which is
 * under the following liberal license: "We have placed this source code in the
 * public domain. Use it in any project, free or commercial."
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

//
//This works pretty well now, and is not on by default.
#define BB_FEATURE_SH_ENVIRONMENT
//
//Backtick support has some problems, use at your own risk!
//#define BB_FEATURE_SH_BACKTICKS
//
//If, then, else, etc. support..  This should now behave basically
//like any other Bourne shell...
//#define BB_FEATURE_SH_IF_EXPRESSIONS
//
/* This is currently a little broken... */
//#define HANDLE_CONTINUATION_CHARS
//
//For debugging/development on the shell only...
//#define DEBUG_SHELL


#include "busybox.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>
#include <getopt.h>
#include "cmdedit.h"

#define MAX_LINE	256	/* size of input buffer for cwd data */
#define MAX_READ	128	/* size of input buffer for `read' builtin */
#define JOB_STATUS_FORMAT "[%d] %-22s %.40s\n"
extern size_t NUM_APPLETS;


enum redir_type { REDIRECT_INPUT, REDIRECT_OVERWRITE,
	REDIRECT_APPEND
};

static const unsigned int DEFAULT_CONTEXT=0x1;
static const unsigned int IF_TRUE_CONTEXT=0x2;
static const unsigned int IF_FALSE_CONTEXT=0x4;
static const unsigned int THEN_EXP_CONTEXT=0x8;
static const unsigned int ELSE_EXP_CONTEXT=0x10;
unsigned int shell_context = 0;



struct jobset {
	struct job *head;			/* head of list of running jobs */
	struct job *fg;				/* current foreground job */
};

struct redir_struct {
	enum redir_type type;	/* type of redirection */
	int fd;						/* file descriptor being redirected */
	char *filename;				/* file to redirect fd to */
};

struct child_prog {
	pid_t pid;					/* 0 if exited */
	char **argv;				/* program name and arguments */
	int num_redirects;			/* elements in redirection array */
	struct redir_struct *redirects;	/* I/O redirects */
	glob_t glob_result;			/* result of parameter globbing */
	int free_glob;				/* should we globfree(&glob_result)? */
	int is_stopped;				/* is the program currently running? */
	struct job *family;			/* pointer back to the child's parent job */
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
static int builtin_env(struct child_prog *dummy);
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
#ifdef BB_FEATURE_SH_IF_EXPRESSIONS
static int builtin_if(struct child_prog *cmd);
static int builtin_then(struct child_prog *cmd);
static int builtin_else(struct child_prog *cmd);
static int builtin_fi(struct child_prog *cmd);
#endif


/* function prototypes for shell stuff */
static void checkjobs(struct jobset *job_list);
static int get_command(FILE * source, char *command);
static int parse_command(char **command_ptr, struct job *job, int *inbg);
static int run_command(struct job *newjob, int inbg, int outpipe[2]);
static int pseudo_exec(struct child_prog *cmd) __attribute__ ((noreturn));
static int run_command_predicate(char *cmd);
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
#ifdef BB_FEATURE_SH_IF_EXPRESSIONS
	{"if", NULL, builtin_if},
	{"then", NULL, builtin_then},
	{"else", NULL, builtin_else},
	{"fi", NULL, builtin_fi},
#endif
	{NULL, NULL, NULL}
};

/* Table of forking built-in functions (things that fork cannot change global
 * variables in the parent process, such as the current working directory) */
static struct built_in_command bltins_forking[] = {
	{"env", "Print all environment variables", builtin_env},
	{"pwd", "Print current directory", builtin_pwd},
	{"help", "List shell built-in commands", builtin_help},
	{NULL, NULL, NULL}
};

static char prompt[3];
static char *cwd;
static char *local_pending_command = NULL;
static char *prompt_str = NULL;
static struct jobset job_list = { NULL, NULL };
static int argc;
static char **argv;
#ifdef BB_FEATURE_SH_ENVIRONMENT
static int last_bg_pid=-1;
static int last_return_code=-1;
static int show_x_trace=FALSE;
#endif
#ifdef BB_FEATURE_SH_IF_EXPRESSIONS
static char syntax_err[]="syntax error near unexpected token";
#endif

#ifdef DEBUG_SHELL
static inline void debug_printf(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}
#else
static inline void debug_printf(const char *format, ...) { }
#endif

#ifdef BB_FEATURE_SH_COMMAND_EDITING
static inline void win_changed(int junk)
{
	struct winsize win = { 0, 0, 0, 0 };
	ioctl(0, TIOCGWINSZ, &win);
	if (win.ws_col > 0) {
		cmdedit_setwidth( win.ws_col - 1);
	}
}
#else
static inline void win_changed(int junk) {}
#endif

/*
	Most builtins need access to the struct child_prog that has
	their arguments, previously coded as cmd->progs[0].  That coding
	can exhibit a bug, if the builtin is not the first command in
	a pipeline: "echo foo | exec sort" will attempt to exec foo.

builtin   previous use      notes
------ -----------------  ---------
cd      cmd->progs[0]
env     0
exec    cmd->progs[0]  squashed bug: didn't look for applets or forking builtins
exit    cmd->progs[0]
fg_bg   cmd->progs[0], job_list->head, job_list->fg
help    0
jobs    job_list->head
pwd     0
export  cmd->progs[0]  passes cmd, job_list to builtin_env(), which ignores them
source  cmd->progs[0]
unset   cmd->progs[0]
read    cmd->progs[0]
if      cmd->job_context,  cmd->text
then    cmd->job_context,  cmd->text
else    cmd->job_context,  cmd->text
fi      cmd->job_context

The use of cmd->text by if/then/else/fi is hopelessly hacky.
Would it work to increment cmd->progs[0]->argv and recurse,
somewhat like builtin_exec does?

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
		printf("cd: %s: %s\n", newdir, strerror(errno));
		return EXIT_FAILURE;
	}
	getcwd(cwd, sizeof(char)*MAX_LINE);

	return EXIT_SUCCESS;
}

/* built-in 'env' handler */
static int builtin_env(struct child_prog *dummy)
{
	char **e;

	for (e = environ; *e; e++) {
		fprintf(stdout, "%s\n", *e);
	}
	return (0);
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

/* built-in 'fg' and 'bg' handler */
static int builtin_fg_bg(struct child_prog *child)
{
	int i, jobNum;
	struct job *job=NULL;
	

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
			perror("tcsetpgrp"); 
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
	for (x = bltins_forking; x->cmd; x++) {
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
static int builtin_pwd(struct child_prog *dummy)
{
	getcwd(cwd, MAX_LINE);
	fprintf(stdout, "%s\n", cwd);
	return EXIT_SUCCESS;
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

#ifdef BB_FEATURE_SH_IF_EXPRESSIONS
/* Built-in handler for 'if' commands */
static int builtin_if(struct child_prog *child)
{
	struct job *cmd = child->family;
	int status;
	char* charptr1=cmd->text+3; /* skip over the leading 'if ' */

	/* Now run the 'if' command */
	debug_printf( "job=%p entering builtin_if ('%s')-- context=%d\n", cmd, charptr1, cmd->job_context);
	status = run_command_predicate(charptr1);
	debug_printf( "if test returned ");
	if (status == 0) {
		debug_printf( "TRUE\n");
		cmd->job_context |= IF_TRUE_CONTEXT;
	} else {
		debug_printf( "FALSE\n");
		cmd->job_context |= IF_FALSE_CONTEXT;
	}
	debug_printf("job=%p builtin_if set job context to %x\n", cmd, cmd->job_context);
	shell_context++;

	return status;
}

/* Built-in handler for 'then' (part of the 'if' command) */
static int builtin_then(struct child_prog *child)
{
	struct job *cmd = child->family;
	char* charptr1=cmd->text+5; /* skip over the leading 'then ' */

	debug_printf( "job=%p entering builtin_then ('%s')-- context=%d\n", cmd, charptr1, cmd->job_context);
	if (! (cmd->job_context & (IF_TRUE_CONTEXT|IF_FALSE_CONTEXT))) {
		shell_context = 0; /* Reset the shell's context on an error */
		error_msg("%s `then'\n", syntax_err);
		return EXIT_FAILURE;
	}

	cmd->job_context |= THEN_EXP_CONTEXT;
	debug_printf("job=%p builtin_then set job context to %x\n", cmd, cmd->job_context);

	/* If the if result was FALSE, skip the 'then' stuff */
	if (cmd->job_context & IF_FALSE_CONTEXT) {
		return EXIT_SUCCESS;
	}

	/* Seems the if result was TRUE, so run the 'then' command */
	debug_printf( "'then' now running '%s'\n", charptr1);

	return(run_command_predicate(charptr1));
}

/* Built-in handler for 'else' (part of the 'if' command) */
static int builtin_else(struct child_prog *child)
{
	struct job *cmd = child->family;
	char* charptr1=cmd->text+5; /* skip over the leading 'else ' */

	debug_printf( "job=%p entering builtin_else ('%s')-- context=%d\n", cmd, charptr1, cmd->job_context);

	if (! (cmd->job_context & THEN_EXP_CONTEXT)) {
		shell_context = 0; /* Reset the shell's context on an error */
		error_msg("%s `else'\n", syntax_err);
		return EXIT_FAILURE;
	}
	/* If the if result was TRUE, skip the 'else' stuff */
	if (cmd->job_context & IF_TRUE_CONTEXT) {
		return EXIT_SUCCESS;
	}

	cmd->job_context |= ELSE_EXP_CONTEXT;
	debug_printf("job=%p builtin_else set job context to %x\n", child->family, cmd->job_context);

	/* Now run the 'else' command */
	debug_printf( "'else' now running '%s'\n", charptr1);
	return(run_command_predicate(charptr1));
}

/* Built-in handler for 'fi' (part of the 'if' command) */
static int builtin_fi(struct child_prog *child)
{
	struct job *cmd = child->family;
	debug_printf( "job=%p entering builtin_fi ('%s')-- context=%d\n", cmd, "", cmd->job_context);
	if (! (cmd->job_context & (IF_TRUE_CONTEXT|IF_FALSE_CONTEXT))) {
		shell_context = 0; /* Reset the shell's context on an error */
		error_msg("%s `fi'\n", syntax_err);
		return EXIT_FAILURE;
	}
	/* Clear out the if and then context bits */
	cmd->job_context &= ~(IF_TRUE_CONTEXT|IF_FALSE_CONTEXT|THEN_EXP_CONTEXT|ELSE_EXP_CONTEXT);
	debug_printf("job=%p builtin_fi set job context to %x\n", cmd, cmd->job_context);
	shell_context--;
	return EXIT_SUCCESS;
}
#endif

/* Built-in '.' handler (read-in and execute commands from file) */
static int builtin_source(struct child_prog *child)
{
	FILE *input;
	int status;

	if (child->argv[1] == NULL)
		return EXIT_FAILURE;

	input = fopen(child->argv[1], "r");
	if (!input) {
		fprintf(stdout, "Couldn't open file '%s'\n", child->argv[1]);
		return EXIT_FAILURE;
	}

	/* Now run the file */
	status = busy_loop(input);
	fclose(input);
	return (status);
}

/* built-in 'unset VAR' handler */
static int builtin_unset(struct child_prog *child)
{
	if (child->argv[1] == NULL) {
		fprintf(stdout, "unset: parameter required.\n");
		return EXIT_FAILURE;
	}
	unsetenv(child->argv[1]);
	return EXIT_SUCCESS;
}

/* currently used by if/then/else.
 * Needlessly (?) forks and reparses the command line.
 * But pseudo_exec on the pre-parsed args doesn't have the
 * "fork, stick around until the child exits, and find it's return code"
 * functionality.  The fork is not needed if the predicate is
 * non-forking builtin, and maybe not even if it's a forking builtin.
 * applets pretty clearly need the fork.
 */
static int run_command_predicate(char *cmd)
{
	int n=strlen(cmd);
	local_pending_command = xmalloc(n+1);
	strncpy(local_pending_command, cmd, n); 
	local_pending_command[n]='\0';
	return( busy_loop(NULL));
}

/* free up all memory from a job */
static void free_job(struct job *cmd)
{
	int i;

	for (i = 0; i < cmd->num_progs; i++) {
		free(cmd->progs[i].argv);
		if (cmd->progs[i].redirects)
			free(cmd->progs[i].redirects);
		if (cmd->progs[i].free_glob)
			globfree(&cmd->progs[i].glob_result);
	}
	free(cmd->progs);
	if (cmd->text)
		free(cmd->text);
	free(cmd->cmdbuf);
	memset(cmd, 0, sizeof(struct job));
}

/* remove a job from the job_list */
static void remove_job(struct jobset *job_list, struct job *job)
{
	struct job *prevjob;

	free_job(job);
	if (job == job_list->head) {
		job_list->head = job->next;
	} else {
		prevjob = job_list->head;
		while (prevjob->next != job)
			prevjob = prevjob->next;
		prevjob->next = job->next;
	}

	free(job);
}

/* Checks to see if any background processes have exited -- if they 
   have, figure out why and see if a job has completed */
static void checkjobs(struct jobset *job_list)
{
	struct job *job;
	pid_t childpid;
	int status;
	int prognum = 0;

	while ((childpid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
		for (job = job_list->head; job; job = job->next) {
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
				remove_job(job_list, job);
			}
		} else {
			/* child stopped */
			job->stopped_progs++;
			job->progs[prognum].is_stopped = 1;

			if (job->stopped_progs == job->num_progs) {
				printf(JOB_STATUS_FORMAT, job->jobid, "Stopped",
					   job->text);
			}
		}
	}

	if (childpid == -1 && errno != ECHILD)
		perror("waitpid");
}

static int setup_redirects(struct child_prog *prog)
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
			error_msg("error opening %s: %s\n", redir->filename,
					strerror(errno));
			return 1;
		}

		if (openfd != redir->fd) {
			dup2(openfd, redir->fd);
			close(openfd);
		}
	}

	return 0;
}


static int get_command(FILE * source, char *command)
{
	char user[9],buf[255],*s;
	
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

	if (shell_context == 0) {
		/* get User Name and setup prompt */
		strcpy(prompt,( geteuid() != 0 ) ? "$ ":"# ");
		my_getpwuid(user, geteuid());
		
		/* get HostName */
		gethostname(buf, 255);
		s = strchr(buf, '.');
		if (s) {
			*s = 0;
		}
	} else {
		strcpy(prompt,"> ");
	}
	
	if (source == stdin) {
#ifdef BB_FEATURE_SH_COMMAND_EDITING
		int len;

		/*
		** enable command line editing only while a command line
		** is actually being read; otherwise, we'll end up bequeathing
		** atexit() handlers and other unwanted stuff to our
		** child processes (rob@sysgo.de)
		*/
		cmdedit_init();
		signal(SIGWINCH, win_changed);
		debug_printf( "in get_command() -- job_context=%d\n", shell_context);
		fflush(stdout);
		if (shell_context == 0) {
			len=fprintf(stdout, "[%s@%s %s]%s", user, buf, 
					get_last_path_component(cwd), prompt);
		} else {
			len=fprintf(stdout, "%s", prompt);
		}
		fflush(stdout);
		prompt_str=(char*)xmalloc(sizeof(char)*(len+1));
		if (shell_context == 0) {
			sprintf(prompt_str, "[%s@%s %s]%s", user, buf, 
					get_last_path_component(cwd), prompt);
		} else {
			sprintf(prompt_str, "%s", prompt);
		}
		cmdedit_read_input(prompt_str, command);
		free( prompt_str);
		cmdedit_terminate();
		signal(SIGWINCH, SIG_DFL);
		return 0;
#else
		i=strlen(cwd);
		i--;
		if (i>1){
			while ((i>0) && (*(cwd+i)!='/') ) i--;
			if (*(cwd+i)=='/') i++;
		}
		
		fprintf(stdout, "[%s@%s %s]%s",user, buf, (cwd+i), prompt);
		fflush(stdout);
#endif
	}

	if (!fgets(command, BUFSIZ - 2, source)) {
		if (source == stdin)
			printf("\n");
		return 1;
	}

	/* remove trailing newline */
	command[strlen(command) - 1] = '\0';

	return 0;
}

#ifdef BB_FEATURE_SH_ENVIRONMENT
static char* itoa(register int i)
{
	static char a[7]; /* Max 7 ints */
	register char *b = a + sizeof(a) - 1;
	int   sign = (i < 0);

	if (sign)
		i = -i;
	*b = 0;
	do
	{
		*--b = '0' + (i % 10);
		i /= 10;
	}
	while (i);
	if (sign)
		*--b = '-';
	return b;
}
#endif

static void expand_argument(struct child_prog *prog, int *argcPtr,
							 int *argv_alloced_ptr)
{
	int argc_l = *argcPtr;
	int argv_alloced = *argv_alloced_ptr;
	int rc;
	int flags;
	int i;
	char *src, *dst, *var;

	if (argc_l > 1) {				/* cmd->glob_result is already initialized */
		flags = GLOB_APPEND;
		i = prog->glob_result.gl_pathc;
	} else {
		prog->free_glob = 1;
		flags = 0;
		i = 0;
	}
	/* do shell variable substitution */
	if(*prog->argv[argc_l - 1] == '$') {
		if ((var = getenv(prog->argv[argc_l - 1] + 1))) {
			prog->argv[argc_l - 1] = var;
		} 
#ifdef BB_FEATURE_SH_ENVIRONMENT
		else {
			switch(*(prog->argv[argc_l - 1] + 1)) {
				case '?':
					prog->argv[argc_l - 1] = itoa(last_return_code);
					break;
				case '$':
					prog->argv[argc_l - 1] = itoa(getpid());
					break;
				case '#':
					prog->argv[argc_l - 1] = itoa(argc-1);
					break;
				case '!':
					if (last_bg_pid==-1)
						*(prog->argv[argc_l - 1])='\0';
					else
						prog->argv[argc_l - 1] = itoa(last_bg_pid);
					break;
				case '0':case '1':case '2':case '3':case '4':
				case '5':case '6':case '7':case '8':case '9':
					{
						int index=*(prog->argv[argc_l - 1] + 1)-48;
						if (index >= argc) {
							*(prog->argv[argc_l - 1])='\0';
						} else {
							prog->argv[argc_l - 1] = argv[index];
						}
					}
					break;
			}
		}
#endif
	}

	if (strpbrk(prog->argv[argc_l - 1],"*[]?")!= NULL){
		rc = glob(prog->argv[argc_l - 1], flags, NULL, &prog->glob_result);
		if (rc == GLOB_NOSPACE) {
			error_msg("out of space during glob operation\n");
			return;
		} else if (rc == GLOB_NOMATCH ||
			   (!rc && (prog->glob_result.gl_pathc - i) == 1 &&
				strcmp(prog->argv[argc_l - 1],
						prog->glob_result.gl_pathv[i]) == 0)) {
			/* we need to remove whatever \ quoting is still present */
			src = dst = prog->argv[argc_l - 1];
			while (*src) {
				if (*src == '\\') {
					src++; 
					*dst++ = process_escape_sequence(&src);
				} else { 
					*dst++ = *src;
					src++;
				}
			}
			*dst = '\0';
		} else if (!rc) {
			argv_alloced += (prog->glob_result.gl_pathc - i);
			prog->argv = xrealloc(prog->argv, argv_alloced * sizeof(*prog->argv));
			memcpy(prog->argv + (argc_l - 1), prog->glob_result.gl_pathv + i,
				   sizeof(*(prog->argv)) * (prog->glob_result.gl_pathc - i));
			argc_l += (prog->glob_result.gl_pathc - i - 1);
		}
	}else{
	 		src = dst = prog->argv[argc_l - 1];
			while (*src) {
				if (*src == '\\') {
					src++; 
					*dst++ = process_escape_sequence(&src);
				} else { 
					*dst++ = *src;
					src++;
				}
			}
			*dst = '\0';
			
			prog->glob_result.gl_pathc=0;
			if (flags==0)
				prog->glob_result.gl_pathv=NULL;
	}
	*argv_alloced_ptr = argv_alloced;
	*argcPtr = argc_l;
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
	char *src, *buf, *chptr;
	int argc_l = 0;
	int done = 0;
	int argv_alloced;
	int i;
	char quote = '\0';
	int count;
	struct child_prog *prog;

	/* skip leading white space */
	while (**command_ptr && isspace(**command_ptr))
		(*command_ptr)++;

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
	job->cmdbuf = command = xcalloc(2*strlen(*command_ptr) + 1, sizeof(char));
	job->text = NULL;

	prog = job->progs;
	prog->num_redirects = 0;
	prog->redirects = NULL;
	prog->free_glob = 0;
	prog->is_stopped = 0;
	prog->family = job;

	argv_alloced = 5;
	prog->argv = xmalloc(sizeof(*prog->argv) * argv_alloced);
	prog->argv[0] = job->cmdbuf;

	buf = command;
	src = *command_ptr;
	while (*src && !done) {
		if (quote == *src) {
			quote = '\0';
		} else if (quote) {
			if (*src == '\\') {
				src++;
				if (!*src) {
					error_msg("character expected after \\\n");
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
			if (*prog->argv[argc_l]) {
				buf++, argc_l++;
				/* +1 here leaves room for the NULL which ends argv */
				if ((argc_l + 1) == argv_alloced) {
					argv_alloced += 5;
					prog->argv = xrealloc(prog->argv,
										  sizeof(*prog->argv) *
										  argv_alloced);
				}
				expand_argument(prog, &argc_l, &argv_alloced);
				prog->argv[argc_l] = buf;
			}
		} else
			switch (*src) {
			case '"':
			case '\'':
				quote = *src;
				break;

			case '#':			/* comment */
				if (*(src-1)== '$')
					*buf++ = *src;
				else
					done = 1;
				break;

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
						expand_argument(prog, &argc_l, &argv_alloced);
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
				while (isspace(*chptr))
					chptr++;

				if (!*chptr) {
					error_msg("file name expected after %c\n", *src);
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
				if (*prog->argv[argc_l])
					argc_l++;
				if (!argc_l) {
					error_msg("empty command in pipe\n");
					free_job(job);
					job->num_progs=0;
					return 1;
				}
				prog->argv[argc_l] = NULL;

				/* and start the next */
				job->num_progs++;
				job->progs = xrealloc(job->progs,
									  sizeof(*job->progs) * job->num_progs);
				prog = job->progs + (job->num_progs - 1);
				prog->num_redirects = 0;
				prog->redirects = NULL;
				prog->free_glob = 0;
				prog->is_stopped = 0;
				prog->family = job;
				argc_l = 0;

				argv_alloced = 5;
				prog->argv = xmalloc(sizeof(*prog->argv) * argv_alloced);
				prog->argv[0] = ++buf;

				src++;
				while (*src && isspace(*src))
					src++;

				if (!*src) {
					error_msg("empty command in pipe\n");
					free_job(job);
					job->num_progs=0;
					return 1;
				}
				src--;			/* we'll ++ it at the end of the loop */

				break;

			case '&':			/* background */
				*inbg = 1;
			case ';':			/* multiple commands */
				done = 1;
				return_command = *command_ptr + (src - *command_ptr) + 1;
				break;

#ifdef BB_FEATURE_SH_BACKTICKS
			case '`':
				/* Exec a backtick-ed command */
				{
					char* charptr1=NULL, *charptr2;
					char* ptr=NULL;
					struct job *newjob;
					struct jobset njob_list = { NULL, NULL };
					int pipefd[2];
					int size;

					ptr=strchr(++src, '`');
					if (ptr==NULL) {
						fprintf(stderr, "Unmatched '`' in command\n");
						free_job(job);
						return 1;
					}

					/* Make some space to hold just the backticked command */
					charptr1 = charptr2 = xmalloc(1+ptr-src);
					memcpy(charptr1, src, ptr-src);
					charptr1[ptr-src] = '\0';
					newjob = xmalloc(sizeof(struct job));
					newjob->job_list = &njob_list;
					/* Now parse and run the backticked command */
					if (!parse_command(&charptr1, newjob, inbg) 
							&& newjob->num_progs) {
						pipe(pipefd);
						run_command(newjob, 0, pipefd);
					}
					checkjobs(job->job_list);
					free_job(newjob);  /* doesn't actually free newjob,
					                     looks like a memory leak */
					free(charptr2);
					
					/* Make a copy of any stuff left over in the command 
					 * line after the second backtick */
					charptr2 = xmalloc(strlen(ptr)+1);
					memcpy(charptr2, ptr+1, strlen(ptr));


					/* Copy the output from the backtick-ed command into the
					 * command line, making extra room as needed  */
					--src;
					charptr1 = xmalloc(BUFSIZ);
					while ( (size=full_read(pipefd[0], charptr1, BUFSIZ-1)) >0) {
						int newsize=src - *command_ptr + size + 1 + strlen(charptr2);
						if (newsize > BUFSIZ) {
							*command_ptr=xrealloc(*command_ptr, newsize);
						}
						memcpy(src, charptr1, size); 
						src+=size;
					}
					free(charptr1);
					close(pipefd[0]);
					if (*(src-1)=='\n')
						--src;

					/* Now paste into the *command_ptr all the stuff 
					 * leftover after the second backtick */
					memcpy(src, charptr2, strlen(charptr2)+1);
					free(charptr2);

					/* Now recursively call parse_command to deal with the new
					 * and improved version of the command line with the backtick
					 * results expanded in place... */
					{
						struct jobset *jl=job->job_list;
						free_job(job);
						job->job_list = jl;
					}
					return(parse_command(command_ptr, job, inbg));
				}
				break;
#endif // BB_FEATURE_SH_BACKTICKS

			case '\\':
				src++;
				if (!*src) {
/* This is currently a little broken... */
#ifdef HANDLE_CONTINUATION_CHARS
					/* They fed us a continuation char, so continue reading stuff
					 * on the next line, then tack that onto the end of the current
					 * command */
					char *command;
					int newsize;
					printf("erik: found a continue char at EOL...\n");
					command = (char *) xcalloc(BUFSIZ, sizeof(char));
					if (get_command(input, command)) {
						error_msg("character expected after \\\n");
						free(command);
						free_job(job);
						return 1;
					}
					newsize = strlen(*command_ptr) + strlen(command) + 2;
					if (newsize > BUFSIZ) {
						printf("erik: doing realloc\n");
						*command_ptr=xrealloc(*command_ptr, newsize);
					}
					printf("erik: A: *command_ptr='%s'\n", *command_ptr);
					memcpy(--src, command, strlen(command)); 
					printf("erik: B: *command_ptr='%s'\n", *command_ptr);
					free(command);
					break;
#else
					error_msg("character expected after \\\n");
					free(command);
					free_job(job);
					return 1;
#endif
				}
				if (*src == '*' || *src == '[' || *src == ']'
					|| *src == '?') *buf++ = '\\';
				/* fallthrough */
			default:
				*buf++ = *src;
			}

		src++;
	}

	if (*prog->argv[argc_l]) {
		argc_l++;
		expand_argument(prog, &argc_l, &argv_alloced);
	}
	if (!argc_l) {
		free_job(job);
		return 0;
	}
	prog->argv[argc_l] = NULL;

	if (!return_command) {
		job->text = xmalloc(strlen(*command_ptr) + 1);
		strcpy(job->text, *command_ptr);
	} else {
		/* This leaves any trailing spaces, which is a bit sloppy */
		count = return_command - *command_ptr;
		job->text = xmalloc(count + 1);
		strncpy(job->text, *command_ptr, count);
		job->text[count] = '\0';
	}

	*command_ptr = return_command;
	
	return 0;
}

/* Run the child_prog, no matter what kind of command it uses.
 */
static int pseudo_exec(struct child_prog *child)
{
	struct built_in_command *x;
#ifdef BB_FEATURE_SH_STANDALONE_SHELL
	struct BB_applet search_applet, *applet;
#endif

	/* Check if the command matches any of the forking builtins.
	 * XXX It would probably be wise to check for non-forking builtins
	 * here as well, since in some context the non-forking path
	 * is disabled or bypassed.  See comment in run_command.
	 */
	for (x = bltins_forking; x->cmd; x++) {
		if (strcmp(child->argv[0], x->cmd) == 0) {
			applet_name=x->cmd;
			exit (x->function(child));
		}
	}
#ifdef BB_FEATURE_SH_STANDALONE_SHELL
	/* Check if the command matches any busybox internal
	 * commands ("applets") here.  Following discussions from
	 * November 2000 on busybox@opensource.lineo.com, don't use
	 * get_last_path_component().  This way explicit (with
	 * slashes) filenames will never be interpreted as an
	 * applet, just like with builtins.  This way the user can
	 * override an applet with an explicit filename reference.
	 * The only downside to this change is that an explicit
	 * /bin/foo invocation will fork and exec /bin/foo, even if
	 * /bin/foo is a symlink to busybox.
	 */
	search_applet.name = child->argv[0];

#ifdef BB_FEATURE_SH_APPLETS_ALWAYS_WIN
	/* If you enable BB_FEATURE_SH_APPLETS_ALWAYS_WIN, then
	 * if you run /bin/cat, it will use BusyBox cat even if 
	 * /bin/cat exists on the filesystem and is _not_ busybox.
	 * Some systems want this, others do not.  Choose wisely.  :-)
	 */
	search_applet.name = get_last_path_component(search_applet.name);
#endif

	/* Do a binary search to find the applet entry given the name. */
	applet = bsearch(&search_applet, applets, NUM_APPLETS,
			sizeof(struct BB_applet), applet_name_compare);
	if (applet != NULL) {
		int argc_l;
		char** argv=child->argv;
		for(argc_l=0;*argv!=NULL; argv++, argc_l++);
		applet_name=applet->name;
		optind = 1;
		exit((*(applet->main)) (argc_l, child->argv));
	}
#endif

	execvp(child->argv[0], child->argv);
	error_msg_and_die("%s: %s\n", child->argv[0],
			strerror(errno));
}

static void insert_job(struct job *newjob, int inbg)
{
	struct job *thejob;
	struct jobset *job_list=newjob->job_list;

	/* find the ID for thejob to use */
	newjob->jobid = 1;
	for (thejob = job_list->head; thejob; thejob = thejob->next)
		if (thejob->jobid >= newjob->jobid)
			newjob->jobid = thejob->jobid + 1;

	/* add thejob to the list of running jobs */
	if (!job_list->head) {
		thejob = job_list->head = xmalloc(sizeof(*thejob));
	} else {
		for (thejob = job_list->head; thejob->next; thejob = thejob->next) /* nothing */;
		thejob->next = xmalloc(sizeof(*thejob));
		thejob = thejob->next;
	}

	*thejob = *newjob;   /* physically copy the struct job */
	thejob->next = NULL;
	thejob->running_progs = thejob->num_progs;
	thejob->stopped_progs = 0;

	if (inbg) {
		/* we don't wait for background thejobs to return -- append it 
		   to the list of backgrounded thejobs and leave it alone */
		printf("[%d] %d\n", thejob->jobid,
			   newjob->progs[newjob->num_progs - 1].pid);
#ifdef BB_FEATURE_SH_ENVIRONMENT
		last_bg_pid=newjob->progs[newjob->num_progs - 1].pid;
#endif
	} else {
		newjob->job_list->fg = thejob;

		/* move the new process group into the foreground */
		/* suppress messages when run from /linuxrc mag@sysgo.de */
		if (tcsetpgrp(0, newjob->pgrp) && errno != ENOTTY)
			perror("tcsetpgrp");
	}
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
			if (pipe(pipefds)<0) perror_msg_and_die("pipe");
			nextout = pipefds[1];
		} else {
			if (outpipe[1]!=-1) {
				nextout = outpipe[1];
			} else {
				nextout = 1;
			}
		}

#ifdef BB_FEATURE_SH_ENVIRONMENT
		if (show_x_trace==TRUE) {
			int j;
			fputc('+', stderr);
			for (j = 0; child->argv[j]; j++) {
				fputc(' ', stderr);
				fputs(child->argv[j], stderr);
			}
			fputc('\n', stderr);
		}
#endif

		/* Check if the command matches any non-forking builtins.
		 * XXX should probably skip this test, and fork anyway, if
		 * there redirects of some kind demand forking to work right.
		 * pseudo_exec would then need to handle the non-forking command
		 * in a forked context.
		 */
		for (x = bltins; x->cmd; x++) {
			if (strcmp(child->argv[0], x->cmd) == 0 ) {
				return(x->function(child));
			}
		}

		if (!(child->pid = fork())) {
			signal(SIGTTOU, SIG_DFL);

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
			setup_redirects(child);

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
	pid_t  parent_pgrp;
	int i;
	int inbg;
	int status;
	newjob.job_list = &job_list;
	newjob.job_context = DEFAULT_CONTEXT;

	/* save current owner of TTY so we can restore it on exit */
	parent_pgrp = tcgetpgrp(0);

	command = (char *) xcalloc(BUFSIZ, sizeof(char));

	/* don't pay any attention to this signal; it just confuses 
	   things and isn't really meant for shells anyway */
	signal(SIGTTOU, SIG_IGN);

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

			if (!parse_command(&next_command, &newjob, &inbg) &&
				newjob.num_progs) {
				int pipefds[2] = {-1,-1};
				debug_printf( "job=%p being fed to run_command by busy_loop()'\n", &newjob);
				run_command(&newjob, inbg, pipefds);
			}
			else {
				free(command);
				command = (char *) xcalloc(BUFSIZ, sizeof(char));
				next_command = NULL;
			}
		} else {
			/* a job is running in the foreground; wait for it */
			i = 0;
			while (!job_list.fg->progs[i].pid ||
				   job_list.fg->progs[i].is_stopped == 1) i++;

			waitpid(job_list.fg->progs[i].pid, &status, WUNTRACED);

			if (WIFEXITED(status) || WIFSIGNALED(status)) {
				/* the child exited */
				job_list.fg->running_progs--;
				job_list.fg->progs[i].pid = 0;

#ifdef BB_FEATURE_SH_ENVIRONMENT
				last_return_code=WEXITSTATUS(status);
#endif
				debug_printf("'%s' exited -- return code %d\n",
						job_list.fg->text, last_return_code);
				if (!job_list.fg->running_progs) {
					/* child exited */
					remove_job(&job_list, job_list.fg);
					job_list.fg = NULL;
				}
			} else {
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
				if (tcsetpgrp(0, getpid()) && errno != ENOTTY)
					perror("tcsetpgrp"); 
			}
		}
	}
	free(command);

	/* return controlling TTY back to parent process group before exiting */
	if (tcsetpgrp(0, parent_pgrp))
		perror("tcsetpgrp");

	/* return exit status if called with "-c" */
	if (input == NULL && WIFEXITED(status))
		return WEXITSTATUS(status);
	
	return 0;
}


#ifdef BB_FEATURE_CLEAN_UP
void free_memory(void)
{
	if (prompt_str)
		free(prompt_str);
	if (cwd)
		free(cwd);
	if (local_pending_command)
		free(local_pending_command);

	if (job_list.fg && !job_list.fg->running_progs) {
		remove_job(&job_list, job_list.fg);
	}
}
#endif


int shell_main(int argc_l, char **argv_l)
{
	int opt, interactive=FALSE;
	FILE *input = stdin;
	argc = argc_l;
	argv = argv_l;


	if (argv[0] && argv[0][0] == '-') {
		  FILE *input;
		  input = fopen("/etc/profile", "r");
		  if (!input) {
			  fprintf(stdout, "Couldn't open file '/etc/profile'\n");
		  } else {
			  /* Now run the file */
			  busy_loop(input);
			  fclose(input);
		  }
	}

	while ((opt = getopt(argc_l, argv_l, "cxi")) > 0) {
		switch (opt) {
			case 'c':
				input = NULL;
				if (local_pending_command != 0)
					error_msg_and_die("multiple -c arguments\n");
				local_pending_command = xstrdup(argv[optind]);
				optind++;
				argv = argv+optind;
				break;
#ifdef BB_FEATURE_SH_ENVIRONMENT
			case 'x':
				show_x_trace = TRUE;
				break;
#endif
			case 'i':
				interactive = TRUE;
				break;
			default:
				usage(shell_usage);
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
		interactive=TRUE;
	}
	if (interactive==TRUE) {
		//fprintf(stdout, "optind=%d  argv[optind]='%s'\n", optind, argv[optind]);
		/* Looks like they want an interactive shell */
		fprintf(stdout, "\n\nBusyBox v%s (%s) Built-in shell (lash)\n", BB_VER, BB_BT);
		fprintf(stdout, "Enter 'help' for a list of built-in commands.\n\n");
	} else if (local_pending_command==NULL) {
		//fprintf(stdout, "optind=%d  argv[optind]='%s'\n", optind, argv[optind]);
		input = xfopen(argv[optind], "r");
	}

	/* initialize the cwd -- this is never freed...*/
	cwd=(char*)xmalloc(sizeof(char)*MAX_LINE+1);
	getcwd(cwd, sizeof(char)*MAX_LINE);

#ifdef BB_FEATURE_CLEAN_UP
	atexit(free_memory);
#endif

	win_changed(0);
	return (busy_loop(input));
}
