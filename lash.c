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


//#define BB_FEATURE_SH_BACKTICKS
//#define BB_FEATURE_SH_IF_EXPRESSIONS
//#define BB_FEATURE_SH_ENVIRONMENT
//#define DEBUG_SHELL


#include "internal.h"
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
#ifdef BB_FEATURE_SH_COMMAND_EDITING
#include "cmdedit.h"
#endif

#define MAX_LINE	256	/* size of input buffer for `read' builtin */
#define MAX_READ	128	/* size of input buffer for `read' builtin */
#define JOB_STATUS_FORMAT "[%d] %-22s %.40s\n"


enum redirectionType { REDIRECT_INPUT, REDIRECT_OVERWRITE,
	REDIRECT_APPEND
};

static const unsigned int REGULAR_JOB_CONTEXT=0x1;
static const unsigned int IF_TRUE_CONTEXT=0x2;
static const unsigned int IF_FALSE_CONTEXT=0x4;
static const unsigned int THEN_EXP_CONTEXT=0x8;
static const unsigned int ELSE_EXP_CONTEXT=0x10;


struct jobSet {
	struct job *head;			/* head of list of running jobs */
	struct job *fg;				/* current foreground job */
};

struct redirectionSpecifier {
	enum redirectionType type;	/* type of redirection */
	int fd;						/* file descriptor being redirected */
	char *filename;				/* file to redirect fd to */
};

struct childProgram {
	pid_t pid;					/* 0 if exited */
	char **argv;				/* program name and arguments */
	int numRedirections;		/* elements in redirection array */
	struct redirectionSpecifier *redirections;	/* I/O redirections */
	glob_t globResult;			/* result of parameter globbing */
	int freeGlob;				/* should we globfree(&globResult)? */
	int isStopped;				/* is the program currently running? */
};

struct job {
	int jobId;					/* job number */
	int numProgs;				/* total number of programs in job */
	int runningProgs;			/* number of programs running */
	char *text;					/* name of job */
	char *cmdBuf;				/* buffer various argv's point into */
	pid_t pgrp;					/* process group ID for the job */
	struct childProgram *progs;	/* array of programs in job */
	struct job *next;			/* to track background commands */
	int stoppedProgs;			/* number of programs alive, but stopped */
	int jobContext;				/* bitmask defining current context */
};

struct builtInCommand {
	char *cmd;					/* name */
	char *descr;				/* description */
	int (*function) (struct job *, struct jobSet * jobList);	/* function ptr */
};

/* function prototypes for builtins */
static int builtin_cd(struct job *cmd, struct jobSet *junk);
static int builtin_env(struct job *dummy, struct jobSet *junk);
static int builtin_exit(struct job *cmd, struct jobSet *junk);
static int builtin_fg_bg(struct job *cmd, struct jobSet *jobList);
static int builtin_help(struct job *cmd, struct jobSet *junk);
static int builtin_jobs(struct job *dummy, struct jobSet *jobList);
static int builtin_pwd(struct job *dummy, struct jobSet *junk);
static int builtin_export(struct job *cmd, struct jobSet *junk);
static int builtin_source(struct job *cmd, struct jobSet *jobList);
static int builtin_unset(struct job *cmd, struct jobSet *junk);
static int builtin_read(struct job *cmd, struct jobSet *junk);
#ifdef BB_FEATURE_SH_IF_EXPRESSIONS
static int builtin_if(struct job *cmd, struct jobSet *junk);
static int builtin_then(struct job *cmd, struct jobSet *junk);
static int builtin_else(struct job *cmd, struct jobSet *junk);
static int builtin_fi(struct job *cmd, struct jobSet *junk);
#endif


/* function prototypes for shell stuff */
static void checkJobs(struct jobSet *jobList);
static int getCommand(FILE * source, char *command);
static int parseCommand(char **commandPtr, struct job *job, struct jobSet *jobList, int *inBg);
static int runCommand(struct job *newJob, struct jobSet *jobList, int inBg, int outPipe[2]);
static int busy_loop(FILE * input);


/* Table of built-in functions (these are non-forking builtins, meaning they
 * can change global variables in the parent shell process but they will not
 * work with pipes and redirects; 'unset foo | whatever' will not work) */
static struct builtInCommand bltins[] = {
	{"bg", "Resume a job in the background", builtin_fg_bg},
	{"cd", "Change working directory", builtin_cd},
	{"exit", "Exit from shell()", builtin_exit},
	{"fg", "Bring job into the foreground", builtin_fg_bg},
	{"jobs", "Lists the active jobs", builtin_jobs},
	{"export", "Set environment variable", builtin_export},
	{"unset", "Unset environment variable", builtin_unset},
	{"read", "Input environment variable", builtin_read},
	{".", "Source-in and run commands in a file", builtin_source},
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
static struct builtInCommand bltins_forking[] = {
	{"env", "Print all environment variables", builtin_env},
	{"pwd", "Print current directory", builtin_pwd},
	{"help", "List shell built-in commands", builtin_help},
	{NULL, NULL, NULL}
};

static char *prompt = "# ";
static char *cwd;
static char *local_pending_command = NULL;
static char *promptStr = NULL;
static struct jobSet jobList = { NULL, NULL };
static int argc;
static char **argv;
#ifdef BB_FEATURE_SH_ENVIRONMENT
static int lastBgPid=-1;
static int lastReturnCode=-1;
static int showXtrace=FALSE;
#endif
	

#ifdef BB_FEATURE_SH_COMMAND_EDITING
void win_changed(int junk)
{
	struct winsize win = { 0, 0, 0, 0 };
	ioctl(0, TIOCGWINSZ, &win);
	if (win.ws_col > 0) {
		cmdedit_setwidth( win.ws_col - 1);
	}
}
#endif


/* built-in 'cd <path>' handler */
static int builtin_cd(struct job *cmd, struct jobSet *junk)
{
	char *newdir;

	if (!cmd->progs[0].argv[1] == 1)
		newdir = getenv("HOME");
	else
		newdir = cmd->progs[0].argv[1];
	if (chdir(newdir)) {
		printf("cd: %s: %s\n", newdir, strerror(errno));
		return FALSE;
	}
	getcwd(cwd, sizeof(char)*MAX_LINE);

	return TRUE;
}

/* built-in 'env' handler */
static int builtin_env(struct job *dummy, struct jobSet *junk)
{
	char **e;

	for (e = environ; *e; e++) {
		fprintf(stdout, "%s\n", *e);
	}
	return (0);
}

/* built-in 'exit' handler */
static int builtin_exit(struct job *cmd, struct jobSet *junk)
{
	if (!cmd->progs[0].argv[1] == 1)
		exit TRUE;

	exit (atoi(cmd->progs[0].argv[1]));
}

/* built-in 'fg' and 'bg' handler */
static int builtin_fg_bg(struct job *cmd, struct jobSet *jobList)
{
	int i, jobNum;
	struct job *job=NULL;

	if (!jobList->head) {
		if (!cmd->progs[0].argv[1] || cmd->progs[0].argv[2]) {
			errorMsg("%s: exactly one argument is expected\n",
					cmd->progs[0].argv[0]);
			return FALSE;
		}
		if (sscanf(cmd->progs[0].argv[1], "%%%d", &jobNum) != 1) {
			errorMsg("%s: bad argument '%s'\n",
					cmd->progs[0].argv[0], cmd->progs[0].argv[1]);
			return FALSE;
			for (job = jobList->head; job; job = job->next) {
				if (job->jobId == jobNum) {
					break;
				}
			}
		}
	} else {
		job = jobList->head;
	}

	if (!job) {
		errorMsg("%s: unknown job %d\n",
				cmd->progs[0].argv[0], jobNum);
		return FALSE;
	}

	if (*cmd->progs[0].argv[0] == 'f') {
		/* Make this job the foreground job */
		/* suppress messages when run from /linuxrc mag@sysgo.de */
		if (tcsetpgrp(0, job->pgrp) && errno != ENOTTY)
			perror("tcsetpgrp"); 
		jobList->fg = job;
	}

	/* Restart the processes in the job */
	for (i = 0; i < job->numProgs; i++)
		job->progs[i].isStopped = 0;

	kill(-job->pgrp, SIGCONT);

	job->stoppedProgs = 0;

	return TRUE;
}

/* built-in 'help' handler */
static int builtin_help(struct job *dummy, struct jobSet *junk)
{
	struct builtInCommand *x;

	fprintf(stdout, "\nBuilt-in commands:\n");
	fprintf(stdout, "-------------------\n");
	for (x = bltins; x->cmd; x++) {
		if (x->descr==NULL)
			continue;
		fprintf(stdout, "%s\t%s\n", x->cmd, x->descr);
	}
	for (x = bltins_forking; x->cmd; x++) {
		if (x->descr==NULL)
			continue;
		fprintf(stdout, "%s\t%s\n", x->cmd, x->descr);
	}
	fprintf(stdout, "\n\n");
	return TRUE;
}

/* built-in 'jobs' handler */
static int builtin_jobs(struct job *dummy, struct jobSet *jobList)
{
	struct job *job;
	char *statusString;

	for (job = jobList->head; job; job = job->next) {
		if (job->runningProgs == job->stoppedProgs)
			statusString = "Stopped";
		else
			statusString = "Running";

		printf(JOB_STATUS_FORMAT, job->jobId, statusString, job->text);
	}
	return TRUE;
}


/* built-in 'pwd' handler */
static int builtin_pwd(struct job *dummy, struct jobSet *junk)
{
	getcwd(cwd, sizeof(char)*MAX_LINE);
	fprintf(stdout, "%s\n", cwd);
	return TRUE;
}

/* built-in 'export VAR=value' handler */
static int builtin_export(struct job *cmd, struct jobSet *junk)
{
	int res;

	if (!cmd->progs[0].argv[1] == 1) {
		return (builtin_env(cmd, junk));
	}
	res = putenv(cmd->progs[0].argv[1]);
	if (res)
		fprintf(stdout, "export: %s\n", strerror(errno));
	return (res);
}

/* built-in 'read VAR' handler */
static int builtin_read(struct job *cmd, struct jobSet *junk)
{
	int res = 0, len, newlen;
	char *s;
	char string[MAX_READ];

	if (cmd->progs[0].argv[1]) {
		/* argument (VAR) given: put "VAR=" into buffer */
		strcpy(string, cmd->progs[0].argv[1]);
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
			fprintf(stdout, "read: %s\n", strerror(errno));
	}
	else
		fgets(string, sizeof(string), stdin);

	return (res);
}

#ifdef BB_FEATURE_SH_IF_EXPRESSIONS
/* Built-in handler for 'if' commands */
static int builtin_if(struct job *cmd, struct jobSet *jobList)
{
	int status;
	char* charptr1=cmd->text+3; /* skip over the leading 'if ' */

	/* Now run the 'if' command */
	status=strlen(charptr1);
	local_pending_command = xmalloc(status+1);
	strncpy(local_pending_command, charptr1, status); 
	local_pending_command[status]='\0';
#ifdef DEBUG_SHELL
	fprintf(stderr, "'if' now testing '%s'\n", local_pending_command);
#endif
	status = busy_loop(NULL); /* Frees local_pending_command */
#ifdef DEBUG_SHELL
	fprintf(stderr, "if test returned ");
#endif
	if (status == 0) {
#ifdef DEBUG_SHELL
		fprintf(stderr, "TRUE\n");
#endif
		cmd->jobContext |= IF_TRUE_CONTEXT;
	} else {
#ifdef DEBUG_SHELL
		fprintf(stderr, "FALSE\n");
#endif
		cmd->jobContext |= IF_FALSE_CONTEXT;
	}

	return status;
}

/* Built-in handler for 'then' (part of the 'if' command) */
static int builtin_then(struct job *cmd, struct jobSet *junk)
{
	int status;
	char* charptr1=cmd->text+5; /* skip over the leading 'then ' */

	if (! (cmd->jobContext & (IF_TRUE_CONTEXT|IF_FALSE_CONTEXT))) {
		errorMsg("unexpected token `then'\n");
		return FALSE;
	}
	/* If the if result was FALSE, skip the 'then' stuff */
	if (cmd->jobContext & IF_FALSE_CONTEXT) {
		return TRUE;
	}

	cmd->jobContext |= THEN_EXP_CONTEXT;
	//printf("Hit an then -- jobContext=%d\n", cmd->jobContext);

	/* Now run the 'then' command */
	status=strlen(charptr1);
	local_pending_command = xmalloc(status+1);
	strncpy(local_pending_command, charptr1, status); 
	local_pending_command[status]='\0';
#ifdef DEBUG_SHELL
	fprintf(stderr, "'then' now running '%s'\n", charptr1);
#endif
	return( busy_loop(NULL));
}

/* Built-in handler for 'else' (part of the 'if' command) */
static int builtin_else(struct job *cmd, struct jobSet *junk)
{
	int status;
	char* charptr1=cmd->text+5; /* skip over the leading 'else ' */

	if (! (cmd->jobContext & (IF_TRUE_CONTEXT|IF_FALSE_CONTEXT))) {
		errorMsg("unexpected token `else'\n");
		return FALSE;
	}
	/* If the if result was TRUE, skip the 'else' stuff */
	if (cmd->jobContext & IF_TRUE_CONTEXT) {
		return TRUE;
	}

	cmd->jobContext |= ELSE_EXP_CONTEXT;
	//printf("Hit an else -- jobContext=%d\n", cmd->jobContext);

	/* Now run the 'else' command */
	status=strlen(charptr1);
	local_pending_command = xmalloc(status+1);
	strncpy(local_pending_command, charptr1, status); 
	local_pending_command[status]='\0';
#ifdef DEBUG_SHELL
	fprintf(stderr, "'else' now running '%s'\n", charptr1);
#endif
	return( busy_loop(NULL));
}

/* Built-in handler for 'fi' (part of the 'if' command) */
static int builtin_fi(struct job *cmd, struct jobSet *junk)
{
	if (! (cmd->jobContext & (IF_TRUE_CONTEXT|IF_FALSE_CONTEXT))) {
		errorMsg("unexpected token `fi'\n");
		return FALSE;
	}
	/* Clear out the if and then context bits */
	cmd->jobContext &= ~(IF_TRUE_CONTEXT|IF_FALSE_CONTEXT|THEN_EXP_CONTEXT|ELSE_EXP_CONTEXT);
#ifdef DEBUG_SHELL
	fprintf(stderr, "Hit an fi   -- jobContext=%d\n", cmd->jobContext);
#endif
	return TRUE;
}
#endif

/* Built-in '.' handler (read-in and execute commands from file) */
static int builtin_source(struct job *cmd, struct jobSet *junk)
{
	FILE *input;
	int status;

	if (!cmd->progs[0].argv[1] == 1)
		return FALSE;

	input = fopen(cmd->progs[0].argv[1], "r");
	if (!input) {
		fprintf(stdout, "Couldn't open file '%s'\n",
				cmd->progs[0].argv[1]);
		return FALSE;
	}

	/* Now run the file */
	status = busy_loop(input);
	fclose(input);
	return (status);
}

/* built-in 'unset VAR' handler */
static int builtin_unset(struct job *cmd, struct jobSet *junk)
{
	if (!cmd->progs[0].argv[1] == 1) {
		fprintf(stdout, "unset: parameter required.\n");
		return FALSE;
	}
	unsetenv(cmd->progs[0].argv[1]);
	return TRUE;
}

/* free up all memory from a job */
static void freeJob(struct job *cmd)
{
	int i;

	for (i = 0; i < cmd->numProgs; i++) {
		free(cmd->progs[i].argv);
		if (cmd->progs[i].redirections)
			free(cmd->progs[i].redirections);
		if (cmd->progs[i].freeGlob)
			globfree(&cmd->progs[i].globResult);
	}
	free(cmd->progs);
	if (cmd->text)
		free(cmd->text);
	free(cmd->cmdBuf);
	memset(cmd, 0, sizeof(struct job));
}

/* remove a job from the jobList */
static void removeJob(struct jobSet *jobList, struct job *job)
{
	struct job *prevJob;

	freeJob(job);
	if (job == jobList->head) {
		jobList->head = job->next;
	} else {
		prevJob = jobList->head;
		while (prevJob->next != job)
			prevJob = prevJob->next;
		prevJob->next = job->next;
	}

	free(job);
}

/* Checks to see if any background processes have exited -- if they 
   have, figure out why and see if a job has completed */
static void checkJobs(struct jobSet *jobList)
{
	struct job *job;
	pid_t childpid;
	int status;
	int progNum = 0;

	while ((childpid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
		for (job = jobList->head; job; job = job->next) {
			progNum = 0;
			while (progNum < job->numProgs &&
				   job->progs[progNum].pid != childpid) progNum++;
			if (progNum < job->numProgs)
				break;
		}

		/* This happens on backticked commands */
		if(job==NULL)
			return;

		if (WIFEXITED(status) || WIFSIGNALED(status)) {
			/* child exited */
			job->runningProgs--;
			job->progs[progNum].pid = 0;

			if (!job->runningProgs) {
				printf(JOB_STATUS_FORMAT, job->jobId, "Done", job->text);
				removeJob(jobList, job);
			}
		} else {
			/* child stopped */
			job->stoppedProgs++;
			job->progs[progNum].isStopped = 1;

			if (job->stoppedProgs == job->numProgs) {
				printf(JOB_STATUS_FORMAT, job->jobId, "Stopped",
					   job->text);
			}
		}
	}

	if (childpid == -1 && errno != ECHILD)
		perror("waitpid");
}

static int setupRedirections(struct childProgram *prog)
{
	int i;
	int openfd;
	int mode = O_RDONLY;
	struct redirectionSpecifier *redir = prog->redirections;

	for (i = 0; i < prog->numRedirections; i++, redir++) {
		switch (redir->type) {
		case REDIRECT_INPUT:
			mode = O_RDONLY;
			break;
		case REDIRECT_OVERWRITE:
			mode = O_RDWR | O_CREAT | O_TRUNC;
			break;
		case REDIRECT_APPEND:
			mode = O_RDWR | O_CREAT | O_APPEND;
			break;
		}

		openfd = open(redir->filename, mode, 0666);
		if (openfd < 0) {
			/* this could get lost if stderr has been redirected, but
			   bash and ash both lose it as well (though zsh doesn't!) */
			errorMsg("error opening %s: %s\n", redir->filename,
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


static int getCommand(FILE * source, char *command)
{
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
		len=fprintf(stdout, "%s %s", cwd, prompt);
		fflush(stdout);
		promptStr=(char*)xmalloc(sizeof(char)*(len+1));
		sprintf(promptStr, "%s %s", cwd, prompt);
		cmdedit_read_input(promptStr, command);
		free( promptStr);
		cmdedit_terminate();
		signal(SIGWINCH, SIG_DFL);
		return 0;
#else
		fprintf(stdout, "%s %s", cwd, prompt);
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
#define __MAX_INT_CHARS 7
static char* itoa(register int i)
{
	static char a[__MAX_INT_CHARS];
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

static void globLastArgument(struct childProgram *prog, int *argcPtr,
							 int *argcAllocedPtr)
{
	int argc_l = *argcPtr;
	int argcAlloced = *argcAllocedPtr;
	int rc;
	int flags;
	int i;
	char *src, *dst, *var;

	if (argc_l > 1) {				/* cmd->globResult is already initialized */
		flags = GLOB_APPEND;
		i = prog->globResult.gl_pathc;
	} else {
		prog->freeGlob = 1;
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
					prog->argv[argc_l - 1] = itoa(lastReturnCode);
					break;
				case '$':
					prog->argv[argc_l - 1] = itoa(getpid());
					break;
				case '#':
					prog->argv[argc_l - 1] = itoa(argc-1);
					break;
				case '!':
					if (lastBgPid==-1)
						*(prog->argv[argc_l - 1])='\0';
					else
						prog->argv[argc_l - 1] = itoa(lastBgPid);
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

	rc = glob(prog->argv[argc_l - 1], flags, NULL, &prog->globResult);
	if (rc == GLOB_NOSPACE) {
		errorMsg("out of space during glob operation\n");
		return;
	} else if (rc == GLOB_NOMATCH ||
			   (!rc && (prog->globResult.gl_pathc - i) == 1 &&
				strcmp(prog->argv[argc_l - 1],
						prog->globResult.gl_pathv[i]) == 0)) {
		/* we need to remove whatever \ quoting is still present */
		src = dst = prog->argv[argc_l - 1];
		while (*src) {
			if (*src != '\\')
				*dst++ = *src;
			src++;
		}
		*dst = '\0';
	} else if (!rc) {
		argcAlloced += (prog->globResult.gl_pathc - i);
		prog->argv = xrealloc(prog->argv, argcAlloced * sizeof(*prog->argv));
		memcpy(prog->argv + (argc_l - 1), prog->globResult.gl_pathv + i,
			   sizeof(*(prog->argv)) * (prog->globResult.gl_pathc - i));
		argc_l += (prog->globResult.gl_pathc - i - 1);
	}

	*argcAllocedPtr = argcAlloced;
	*argcPtr = argc_l;
}

/* Return cmd->numProgs as 0 if no command is present (e.g. an empty
   line). If a valid command is found, commandPtr is set to point to
   the beginning of the next command (if the original command had more 
   then one job associated with it) or NULL if no more commands are 
   present. */
static int parseCommand(char **commandPtr, struct job *job, struct jobSet *jobList, int *inBg)
{
	char *command;
	char *returnCommand = NULL;
	char *src, *buf, *chptr;
	int argc_l = 0;
	int done = 0;
	int argvAlloced;
	int i;
	char quote = '\0';
	int count;
	struct childProgram *prog;

	/* skip leading white space */
	while (**commandPtr && isspace(**commandPtr))
		(*commandPtr)++;

	/* this handles empty lines or leading '#' characters */
	if (!**commandPtr || (**commandPtr == '#')) {
		job->numProgs=0;
		return 0;
	}

	*inBg = 0;
	job->numProgs = 1;
	job->progs = xmalloc(sizeof(*job->progs));

	/* We set the argv elements to point inside of this string. The 
	   memory is freed by freeJob(). Allocate twice the original
	   length in case we need to quote every single character.

	   Getting clean memory relieves us of the task of NULL 
	   terminating things and makes the rest of this look a bit 
	   cleaner (though it is, admittedly, a tad less efficient) */
	job->cmdBuf = command = xcalloc(2*strlen(*commandPtr) + 1, sizeof(char));
	job->text = NULL;

	prog = job->progs;
	prog->numRedirections = 0;
	prog->redirections = NULL;
	prog->freeGlob = 0;
	prog->isStopped = 0;

	argvAlloced = 5;
	prog->argv = xmalloc(sizeof(*prog->argv) * argvAlloced);
	prog->argv[0] = job->cmdBuf;

	buf = command;
	src = *commandPtr;
	while (*src && !done) {
		if (quote == *src) {
			quote = '\0';
		} else if (quote) {
			if (*src == '\\') {
				src++;
				if (!*src) {
					errorMsg("character expected after \\\n");
					freeJob(job);
					return 1;
				}

				/* in shell, "\'" should yield \' */
				if (*src != quote)
					*buf++ = '\\';
			} else if (*src == '*' || *src == '?' || *src == '[' ||
					   *src == ']') *buf++ = '\\';
			*buf++ = *src;
		} else if (isspace(*src)) {
			if (*prog->argv[argc_l]) {
				buf++, argc_l++;
				/* +1 here leaves room for the NULL which ends argv */
				if ((argc_l + 1) == argvAlloced) {
					argvAlloced += 5;
					prog->argv = xrealloc(prog->argv,
										  sizeof(*prog->argv) *
										  argvAlloced);
				}
				globLastArgument(prog, &argc_l, &argvAlloced);
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

			case '>':			/* redirections */
			case '<':
				i = prog->numRedirections++;
				prog->redirections = xrealloc(prog->redirections,
											  sizeof(*prog->redirections) *
											  (i + 1));

				prog->redirections[i].fd = -1;
				if (buf != prog->argv[argc_l]) {
					/* the stuff before this character may be the file number 
					   being redirected */
					prog->redirections[i].fd =
						strtol(prog->argv[argc_l], &chptr, 10);

					if (*chptr && *prog->argv[argc_l]) {
						buf++, argc_l++;
						globLastArgument(prog, &argc_l, &argvAlloced);
						prog->argv[argc_l] = buf;
					}
				}

				if (prog->redirections[i].fd == -1) {
					if (*src == '>')
						prog->redirections[i].fd = 1;
					else
						prog->redirections[i].fd = 0;
				}

				if (*src++ == '>') {
					if (*src == '>')
						prog->redirections[i].type =
							REDIRECT_APPEND, src++;
					else
						prog->redirections[i].type = REDIRECT_OVERWRITE;
				} else {
					prog->redirections[i].type = REDIRECT_INPUT;
				}

				/* This isn't POSIX sh compliant. Oh well. */
				chptr = src;
				while (isspace(*chptr))
					chptr++;

				if (!*chptr) {
					errorMsg("file name expected after %c\n", *src);
					freeJob(job);
					job->numProgs=0;
					return 1;
				}

				prog->redirections[i].filename = buf;
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
					errorMsg("empty command in pipe\n");
					freeJob(job);
					job->numProgs=0;
					return 1;
				}
				prog->argv[argc_l] = NULL;

				/* and start the next */
				job->numProgs++;
				job->progs = xrealloc(job->progs,
									  sizeof(*job->progs) * job->numProgs);
				prog = job->progs + (job->numProgs - 1);
				prog->numRedirections = 0;
				prog->redirections = NULL;
				prog->freeGlob = 0;
				prog->isStopped = 0;
				argc_l = 0;

				argvAlloced = 5;
				prog->argv = xmalloc(sizeof(*prog->argv) * argvAlloced);
				prog->argv[0] = ++buf;

				src++;
				while (*src && isspace(*src))
					src++;

				if (!*src) {
					errorMsg("empty command in pipe\n");
					freeJob(job);
					job->numProgs=0;
					return 1;
				}
				src--;			/* we'll ++ it at the end of the loop */

				break;

			case '&':			/* background */
				*inBg = 1;
			case ';':			/* multiple commands */
				done = 1;
				returnCommand = *commandPtr + (src - *commandPtr) + 1;
				break;

#ifdef BB_FEATURE_SH_BACKTICKS
			case '`':
				/* Exec a backtick-ed command */
				{
					char* charptr1=NULL, *charptr2;
					char* ptr=NULL;
					struct job *newJob;
					struct jobSet njobList = { NULL, NULL };
					int pipefd[2];
					int size;

					ptr=strchr(++src, '`');
					if (ptr==NULL) {
						fprintf(stderr, "Unmatched '`' in command\n");
						freeJob(job);
						return 1;
					}

					/* Make some space to hold just the backticked command */
					charptr1 = charptr2 = xmalloc(1+ptr-src);
					snprintf(charptr1, 1+ptr-src, src);
					newJob = xmalloc(sizeof(struct job));
					/* Now parse and run the backticked command */
					if (!parseCommand(&charptr1, newJob, &njobList, inBg) 
							&& newJob->numProgs) {
						pipe(pipefd);
						runCommand(newJob, &njobList, 0, pipefd);
					}
					checkJobs(jobList);
					freeJob(newJob);
					free(charptr2);
					
					/* Make a copy of any stuff left over in the command 
					 * line after the second backtick */
					charptr2 = xmalloc(strlen(ptr)+1);
					memcpy(charptr2, ptr+1, strlen(ptr));


					/* Copy the output from the backtick-ed command into the
					 * command line, making extra room as needed  */
					--src;
					charptr1 = xmalloc(BUFSIZ);
					while ( (size=fullRead(pipefd[0], charptr1, BUFSIZ-1)) >0) {
						int newSize=src - *commandPtr + size + 1 + strlen(charptr2);
						if (newSize > BUFSIZ) {
							*commandPtr=xrealloc(*commandPtr, src - *commandPtr + 
									size + 1 + strlen(charptr2));
						}
						memcpy(src, charptr1, size); 
						src+=size;
					}
					free(charptr1);
					close(pipefd[0]);
					if (*(src-1)=='\n')
						--src;

					/* Now paste into the *commandPtr all the stuff 
					 * leftover after the second backtick */
					memcpy(src, charptr2, strlen(charptr2)+1);
					free(charptr2);

					/* Now recursively call parseCommand to deal with the new
					 * and improved version of the command line with the backtick
					 * results expanded in place... */
					freeJob(job);
					return(parseCommand(commandPtr, job, jobList, inBg));
				}
				break;
#endif // BB_FEATURE_SH_BACKTICKS

			case '\\':
				src++;
				if (!*src) {
					errorMsg("character expected after \\\n");
					freeJob(job);
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

	if (*prog->argv[argc_l]) {
		argc_l++;
		globLastArgument(prog, &argc_l, &argvAlloced);
	}
	if (!argc_l) {
		freeJob(job);
		return 0;
	}
	prog->argv[argc_l] = NULL;

	if (!returnCommand) {
		job->text = xmalloc(strlen(*commandPtr) + 1);
		strcpy(job->text, *commandPtr);
	} else {
		/* This leaves any trailing spaces, which is a bit sloppy */
		count = returnCommand - *commandPtr;
		job->text = xmalloc(count + 1);
		strncpy(job->text, *commandPtr, count);
		job->text[count] = '\0';
	}

	*commandPtr = returnCommand;

	return 0;
}

static int runCommand(struct job *newJob, struct jobSet *jobList, int inBg, int outPipe[2])
{
	struct job *theJob;
	int i;
	int nextin, nextout;
	int pipefds[2];				/* pipefd[0] is for reading */
	struct builtInCommand *x;
#ifdef BB_FEATURE_SH_STANDALONE_SHELL
	const struct BB_applet *a = applets;
#endif


	nextin = 0, nextout = 1;
	for (i = 0; i < newJob->numProgs; i++) {
		if ((i + 1) < newJob->numProgs) {
			pipe(pipefds);
			nextout = pipefds[1];
		} else {
			if (outPipe[1]!=-1) {
				nextout = outPipe[1];
			} else {
				nextout = 1;
			}
		}

#ifdef BB_FEATURE_SH_ENVIRONMENT
		if (showXtrace==TRUE) {
			int j;
			fprintf(stderr, "+ ");
			for (j = 0; newJob->progs[i].argv[j]; j++)
				fprintf(stderr, "%s ", newJob->progs[i].argv[j]);
			fprintf(stderr, "\n");
		}
#endif

		/* Check if the command matches any non-forking builtins */
		for (x = bltins; x->cmd; x++) {
			if (strcmp(newJob->progs[i].argv[0], x->cmd) == 0 ) {
				return(x->function(newJob, jobList));
			}
		}

		if (!(newJob->progs[i].pid = fork())) {
			signal(SIGTTOU, SIG_DFL);

			if (outPipe[1]!=-1) {
				close(outPipe[0]);
			}
			if (nextin != 0) {
				dup2(nextin, 0);
				close(nextin);
			}

			if (nextout != 1) {
				dup2(nextout, 1);
				dup2(nextout, 2);
				close(nextout);
				close(pipefds[0]);
			}

			/* explicit redirections override pipes */
			setupRedirections(newJob->progs + i);

			/* Check if the command matches any of the other builtins */
			for (x = bltins_forking; x->cmd; x++) {
				if (strcmp(newJob->progs[i].argv[0], x->cmd) == 0) {
					applet_name=x->cmd;
					exit (x->function(newJob, jobList));
				}
			}
#ifdef BB_FEATURE_SH_STANDALONE_SHELL
			/* Check if the command matches any busybox internal commands here */
			while (a->name != 0) {
				if (strcmp(get_last_path_component(newJob->progs[i].argv[0]), a->name) == 0) {
					int argc_l;
					char** argv=newJob->progs[i].argv;
					for(argc_l=0;*argv!=NULL; argv++, argc_l++);
					applet_name=a->name;
					optind = 1;
					exit((*(a->main)) (argc_l, newJob->progs[i].argv));
				}
				a++;
			}
#endif

			execvp(newJob->progs[i].argv[0], newJob->progs[i].argv);
			fatalError("%s: %s\n", newJob->progs[i].argv[0],
					   strerror(errno));
		}
		if (outPipe[1]!=-1) {
			close(outPipe[1]);
		}

		/* put our child in the process group whose leader is the
		   first process in this pipe */
		setpgid(newJob->progs[i].pid, newJob->progs[0].pid);
		if (nextin != 0)
			close(nextin);
		if (nextout != 1)
			close(nextout);

		/* If there isn't another process, nextin is garbage 
		   but it doesn't matter */
		nextin = pipefds[0];
	}

	newJob->pgrp = newJob->progs[0].pid;

	/* find the ID for the theJob to use */
	newJob->jobId = 1;
	for (theJob = jobList->head; theJob; theJob = theJob->next)
		if (theJob->jobId >= newJob->jobId)
			newJob->jobId = theJob->jobId + 1;

	/* add the theJob to the list of running jobs */
	if (!jobList->head) {
		theJob = jobList->head = xmalloc(sizeof(*theJob));
	} else {
		for (theJob = jobList->head; theJob->next; theJob = theJob->next);
		theJob->next = xmalloc(sizeof(*theJob));
		theJob = theJob->next;
	}

	*theJob = *newJob;
	theJob->next = NULL;
	theJob->runningProgs = theJob->numProgs;
	theJob->stoppedProgs = 0;

	if (inBg) {
		/* we don't wait for background theJobs to return -- append it 
		   to the list of backgrounded theJobs and leave it alone */
		printf("[%d] %d\n", theJob->jobId,
			   newJob->progs[newJob->numProgs - 1].pid);
#ifdef BB_FEATURE_SH_ENVIRONMENT
		lastBgPid=newJob->progs[newJob->numProgs - 1].pid;
#endif
	} else {
		jobList->fg = theJob;

		/* move the new process group into the foreground */
		/* suppress messages when run from /linuxrc mag@sysgo.de */
		if (tcsetpgrp(0, newJob->pgrp) && errno != ENOTTY)
			perror("tcsetpgrp");
	}

	return 0;
}

static int busy_loop(FILE * input)
{
	char *command;
	char *nextCommand = NULL;
	struct job newJob;
	pid_t  parent_pgrp;
	int i;
	int inBg;
	int status;
	newJob.jobContext = REGULAR_JOB_CONTEXT;

	/* save current owner of TTY so we can restore it on exit */
	parent_pgrp = tcgetpgrp(0);

	command = (char *) xcalloc(BUFSIZ, sizeof(char));

	/* don't pay any attention to this signal; it just confuses 
	   things and isn't really meant for shells anyway */
	signal(SIGTTOU, SIG_IGN);

	while (1) {
		if (!jobList.fg) {
			/* no job is in the foreground */

			/* see if any background processes have exited */
			checkJobs(&jobList);

			if (!nextCommand) {
				if (getCommand(input, command))
					break;
				nextCommand = command;
			}

			if (!parseCommand(&nextCommand, &newJob, &jobList, &inBg) &&
				newJob.numProgs) {
				int pipefds[2] = {-1,-1};
				runCommand(&newJob, &jobList, inBg, pipefds);
			}
			else {
				free(command);
				command = (char *) xcalloc(BUFSIZ, sizeof(char));
				nextCommand = NULL;
			}
		} else {
			/* a job is running in the foreground; wait for it */
			i = 0;
			while (!jobList.fg->progs[i].pid ||
				   jobList.fg->progs[i].isStopped == 1) i++;

			waitpid(jobList.fg->progs[i].pid, &status, WUNTRACED);

			if (WIFEXITED(status) || WIFSIGNALED(status)) {
				/* the child exited */
				jobList.fg->runningProgs--;
				jobList.fg->progs[i].pid = 0;

#ifdef BB_FEATURE_SH_ENVIRONMENT
				lastReturnCode=WEXITSTATUS(status);
#endif
#if 0
				printf("'%s' exited -- return code %d\n", jobList.fg->text, lastReturnCode);
#endif
				if (!jobList.fg->runningProgs) {
					/* child exited */

					removeJob(&jobList, jobList.fg);
					jobList.fg = NULL;
				}
			} else {
				/* the child was stopped */
				jobList.fg->stoppedProgs++;
				jobList.fg->progs[i].isStopped = 1;

				if (jobList.fg->stoppedProgs == jobList.fg->runningProgs) {
					printf("\n" JOB_STATUS_FORMAT, jobList.fg->jobId,
						   "Stopped", jobList.fg->text);
					jobList.fg = NULL;
				}
			}

			if (!jobList.fg) {
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
	if (promptStr)
		free(promptStr);
	if (cwd)
		free(cwd);
	if (local_pending_command)
		free(local_pending_command);

	if (jobList.fg && !jobList.fg->runningProgs) {
		removeJob(&jobList, jobList.fg);
	}
}
#endif


int shell_main(int argc_l, char **argv_l)
{
	int opt, interactive=FALSE;
	FILE *input = stdin;
	argc = argc_l;
	argv = argv_l;


	//if (argv[0] && argv[0][0] == '-') {
	//      builtin_source("/etc/profile");
	//}

	while ((opt = getopt(argc_l, argv_l, "cx")) > 0) {
		switch (opt) {
			case 'c':
				input = NULL;
				if (local_pending_command != 0)
					fatalError("multiple -c arguments\n");
				local_pending_command = xstrdup(argv[optind]);
				optind++;
				argv = argv+optind;
				break;
#ifdef BB_FEATURE_SH_ENVIRONMENT
			case 'x':
				showXtrace = TRUE;
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
	if (interactive==TRUE || ( argv[optind]==NULL && input==stdin && isatty(fileno(stdin)) && isatty(fileno(stdout)))) {
		//fprintf(stdout, "optind=%d  argv[optind]='%s'\n", optind, argv[optind]);
		/* Looks like they want an interactive shell */
		fprintf(stdout, "\n\nBusyBox v%s (%s) Built-in shell\n", BB_VER, BB_BT);
		fprintf(stdout, "Enter 'help' for a list of built-in commands.\n\n");
	} else if (local_pending_command==NULL) {
		//fprintf(stdout, "optind=%d  argv[optind]='%s'\n", optind, argv[optind]);
		input = fopen(argv[optind], "r");
		if (!input) {
			fatalError("%s: %s\n", argv[optind], strerror(errno));
		}
	}

	/* initialize the cwd -- this is never freed...*/
	cwd=(char*)xmalloc(sizeof(char)*MAX_LINE+1);
	getcwd(cwd, sizeof(char)*MAX_LINE);

#ifdef BB_FEATURE_CLEAN_UP
	atexit(free_memory);
#endif

#ifdef BB_FEATURE_SH_COMMAND_EDITING
	win_changed(0);
#endif

	return (busy_loop(input));
}
