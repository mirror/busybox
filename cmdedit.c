/* vi: set sw=4 ts=4: */
/*
 * Termios command line History and Editting, originally 
 * intended for NetBSD sh (ash)
 * Copyright (c) 1999
 *      Main code:            Adam Rogoyski <rogoyski@cs.utexas.edu> 
 *      Etc:                  Dave Cinege <dcinege@psychosis.com>
 *  Majorly adjusted/re-written for busybox:
 *                            Erik Andersen <andersee@debian.org>
 *
 * You may use this code as you wish, so long as the original author(s)
 * are attributed in any redistributions of the source code.
 * This code is 'as is' with no warranty.
 * This code may safely be consumed by a BSD or GPL license.
 *
 * v 0.5  19990328      Initial release 
 *
 * Future plans: Simple file and path name completion. (like BASH)
 *
 */

/*
   Usage and Known bugs:
   Terminal key codes are not extensive, and more will probably
   need to be added. This version was created on Debian GNU/Linux 2.x.
   Delete, Backspace, Home, End, and the arrow keys were tested
   to work in an Xterm and console. Ctrl-A also works as Home.
   Ctrl-E also works as End. The binary size increase is <3K.

   Editting will not display correctly for lines greater then the 
   terminal width. (more then one line.) However, history will.
 */

#include "internal.h"
#ifdef BB_FEATURE_SH_COMMAND_EDITING

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <signal.h>


#define  MAX_HISTORY   15		/* Maximum length of the linked list for the command line history */

#define ESC	27
#define DEL	127
#define member(c, s) ((c) ? ((char *)strchr ((s), (c)) != (char *)NULL) : 0)
#define whitespace(c) (((c) == ' ') || ((c) == '\t'))

static struct history *his_front = NULL;	/* First element in command line list */
static struct history *his_end = NULL;	/* Last element in command line list */

/* ED: sparc termios is broken: revert back to old termio handling. */
#ifdef BB_FEATURE_USE_TERMIOS

#if #cpu(sparc)
#      include <termio.h>
#      define termios termio
#      define setTermSettings(fd,argp) ioctl(fd,TCSETAF,argp)
#      define getTermSettings(fd,argp) ioctl(fd,TCGETA,argp)
#else
#      include <termios.h>
#      define setTermSettings(fd,argp) tcsetattr(fd,TCSANOW,argp)
#      define getTermSettings(fd,argp) tcgetattr(fd, argp);
#endif

/* Current termio and the previous termio before starting sh */
struct termios initial_settings, new_settings;
#endif



static int cmdedit_termw = 80;  /* actual terminal width */
static int cmdedit_scroll = 27; /* width of EOL scrolling region */
static int history_counter = 0;	/* Number of commands in history list */
static int reset_term = 0;		/* Set to true if the terminal needs to be reset upon exit */

struct history {
	char *s;
	struct history *p;
	struct history *n;
};

#define xwrite write

void
cmdedit_setwidth(int w)
{
	if (w > 20) {
		cmdedit_termw = w;
		cmdedit_scroll = w / 3;
	} else {
		errorMsg("\n*** Error: minimum screen width is 21\n");
	}
}


void cmdedit_reset_term(void)
{
	if (reset_term)
		/* sparc and other have broken termios support: use old termio handling. */
		setTermSettings(fileno(stdin), (void*) &initial_settings);
}

void clean_up_and_die(int sig)
{
	cmdedit_reset_term();
	fprintf(stdout, "\n");
	if (sig!=SIGINT)
		exit(TRUE);
}

/* Go to HOME position */
void input_home(int outputFd, int *cursor)
{
	while (*cursor > 0) {
		xwrite(outputFd, "\b", 1);
		--*cursor;
	}
}

/* Go to END position */
void input_end(int outputFd, int *cursor, int len)
{
	while (*cursor < len) {
		xwrite(outputFd, "\033[C", 3);
		++*cursor;
	}
}

/* Delete the char in back of the cursor */
void input_backspace(char* command, int outputFd, int *cursor, int *len)
{
	int j = 0;

	if (*cursor > 0) {
		xwrite(outputFd, "\b \b", 3);
		--*cursor;
		memmove(command + *cursor, command + *cursor + 1,
				BUFSIZ - *cursor + 1);

		for (j = *cursor; j < (BUFSIZ - 1); j++) {
			if (!*(command + j))
				break;
			else
				xwrite(outputFd, (command + j), 1);
		}

		xwrite(outputFd, " \b", 2);

		while (j-- > *cursor)
			xwrite(outputFd, "\b", 1);

		--*len;
	}
}

/* Delete the char in front of the cursor */
void input_delete(char* command, int outputFd, int cursor, int *len)
{
	int j = 0;

	if (cursor == *len)
		return;
	
	memmove(command + cursor, command + cursor + 1,
			BUFSIZ - cursor - 1);
	for (j = cursor; j < (BUFSIZ - 1); j++) {
		if (!*(command + j))
			break;
		else
			xwrite(outputFd, (command + j), 1);
	}

	xwrite(outputFd, " \b", 2);

	while (j-- > cursor)
		xwrite(outputFd, "\b", 1);
	--*len;
}

/* Move forward one charactor */
void input_forward(int outputFd, int *cursor, int len)
{
	if (*cursor < len) {
		xwrite(outputFd, "\033[C", 3);
		++*cursor;
	}
}

/* Move back one charactor */
void input_backward(int outputFd, int *cursor)
{
	if (*cursor > 0) {
		xwrite(outputFd, "\033[D", 3);
		--*cursor;
	}
}



#ifdef BB_FEATURE_SH_TAB_COMPLETION
char** username_tab_completion(char* command, int *num_matches)
{
	char **matches = (char **) NULL;
	*num_matches=0;
	fprintf(stderr, "\nin username_tab_completion\n");
	return (matches);
}

#include <dirent.h>
char** exe_n_cwd_tab_completion(char* command, int *num_matches)
{
	char *dirName;
	char **matches = (char **) NULL;
	DIR *dir;
	struct dirent *next;
			
	matches = malloc( sizeof(char*)*50);

	/* Stick a wildcard onto the command, for later use */
	strcat( command, "*");

	/* Now wall the current directory */
	dirName = get_current_dir_name();
	dir = opendir(dirName);
	if (!dir) {
		/* Don't print an error, just shut up and return */
		*num_matches=0;
		return (matches);
	}
	while ((next = readdir(dir)) != NULL) {

		/* Some quick sanity checks */
		if ((strcmp(next->d_name, "..") == 0)
			|| (strcmp(next->d_name, ".") == 0)) {
			continue;
		} 
		/* See if this matches */
		if (check_wildcard_match(next->d_name, command) == TRUE) {
			/* Cool, found a match.  Add it to the list */
			matches[*num_matches] = malloc(strlen(next->d_name)+1);
			strcpy( matches[*num_matches], next->d_name);
			++*num_matches;
			//matches = realloc( matches, sizeof(char*)*(*num_matches));
		}
	}

	return (matches);
}

void input_tab(char* command, char* prompt, int outputFd, int *cursor, int *len)
{
	/* Do TAB completion */
	static int num_matches=0;
	static char **matches = (char **) NULL;
	int pos = cursor;


	if (lastWasTab == FALSE) {
		char *tmp, *tmp1, *matchBuf;

		/* For now, we will not bother with trying to distinguish
		 * whether the cursor is in/at a command extression -- we
		 * will always try all possable matches.  If you don't like
		 * that then feel free to fix it.
		 */

		/* Make a local copy of the string -- up 
		 * to the position of the cursor */
		matchBuf = (char *) calloc(BUFSIZ, sizeof(char));
		strncpy(matchBuf, command, cursor);
		tmp=matchBuf;

		/* skip past any command seperator tokens */
		while (*tmp && (tmp1=strpbrk(tmp, ";|&{(`")) != NULL) {
			tmp=++tmp1;
			/* skip any leading white space */
			while (*tmp && isspace(*tmp)) 
				++tmp;
		}

		/* skip any leading white space */
		while (*tmp && isspace(*tmp)) 
			++tmp;

		/* Free up any memory already allocated */
		if (matches) {
			free(matches);
			matches = (char **) NULL;
		}

		/* If the word starts with `~' and there is no slash in the word, 
		 * then try completing this word as a username. */

		/* FIXME -- this check is broken! */
		if (*tmp == '~' && !strchr(tmp, '/'))
			matches = username_tab_completion(tmp, &num_matches);

		/* Try to match any executable in our path and everything 
		 * in the current working directory that matches.  */
		if (!matches)
			matches = exe_n_cwd_tab_completion(tmp, &num_matches);

		/* Don't leak memory */
		free( matchBuf);

		/* Did we find exactly one match? */
		if (matches && num_matches==1) {
			/* write out the matched command */
			strncpy(command+pos, matches[0]+pos, strlen(matches[0])-pos);
			len=strlen(command);
			cursor=len;
			xwrite(outputFd, matches[0]+pos, strlen(matches[0])-pos);
			break;
		}
	} else {
		/* Ok -- the last char was a TAB.  Since they
		 * just hit TAB again, print a list of all the
		 * available choices... */
		if ( matches && num_matches>0 ) {
			int i, col;

			/* Go to the next line */
			xwrite(outputFd, "\n", 1);
			/* Print the list of matches */
			for (i=0,col=0; i<num_matches; i++) {
				char foo[17];
				sprintf(foo, "%-14s  ", matches[i]);
				col += xwrite(outputFd, foo, strlen(foo));
				if (col > 60 && matches[i+1] != NULL) {
					xwrite(outputFd, "\n", 1);
					col = 0;
				}
			}
			/* Go to the next line */
			xwrite(outputFd, "\n", 1);
			/* Rewrite the prompt */
			xwrite(outputFd, prompt, strlen(prompt));
			/* Rewrite the command */
			xwrite(outputFd, command, len);
			/* Put the cursor back to where it used to be */
			for (cursor=len; cursor > pos; cursor--)
				xwrite(outputFd, "\b", 1);
		}
	}
}
#endif

void get_previous_history(struct history **hp, char* command)
{
	free((*hp)->s);
	(*hp)->s = strdup(command);
	*hp = (*hp)->p;
}

void get_next_history(struct history **hp, char* command)
{
	free((*hp)->s);
	(*hp)->s = strdup(command);
	*hp = (*hp)->n;
}

/*
 * This function is used to grab a character buffer
 * from the input file descriptor and allows you to
 * a string with full command editing (sortof like
 * a mini readline).
 *
 * The following standard commands are not implemented:
 * ESC-b -- Move back one word
 * ESC-f -- Move forward one word
 * ESC-d -- Delete back one word
 * ESC-h -- Delete forward one word
 * CTL-t -- Transpose two characters
 *
 * Furthermore, the "vi" command editing keys are not implemented.
 *
 * TODO: implement TAB command completion. :)
 */
extern void cmdedit_read_input(char* prompt, char command[BUFSIZ])
{

	int inputFd=fileno(stdin);
	int outputFd=fileno(stdout);
	int nr = 0;
	int len = 0;
	int j = 0;
	int cursor = 0;
	int break_out = 0;
	int ret = 0;
	int lastWasTab = FALSE;
	char c = 0;
	struct history *hp = his_end;

	memset(command, 0, sizeof(command));
	if (!reset_term) {
		
		getTermSettings(inputFd, (void*) &initial_settings);
		memcpy(&new_settings, &initial_settings, sizeof(struct termios));
		new_settings.c_cc[VMIN] = 1;
		new_settings.c_cc[VTIME] = 0;
		new_settings.c_cc[VINTR] = _POSIX_VDISABLE; /* Turn off CTRL-C, so we can trap it */
		new_settings.c_lflag &= ~ICANON;	/* unbuffered input */
		new_settings.c_lflag &= ~(ECHO|ECHOCTL|ECHONL); /* Turn off echoing */
		reset_term = 1;
	}
	setTermSettings(inputFd, (void*) &new_settings);

	memset(command, 0, BUFSIZ);

	while (1) {

		if ((ret = read(inputFd, &c, 1)) < 1)
			return;
		//fprintf(stderr, "got a '%c' (%d)\n", c, c);

		switch (c) {
		case '\n':
		case '\r':
			/* Enter */
			*(command + len++ + 1) = c;
			xwrite(outputFd, &c, 1);
			break_out = 1;
			break;
		case 1:
			/* Control-a -- Beginning of line */
			input_home(outputFd, &cursor);
		case 2:
			/* Control-b -- Move back one character */
			input_backward(outputFd, &cursor);
			break;
		case 3:
			/* Control-c -- leave the current line, 
			 * and start over on the next line */ 

			/* Go to the next line */
			xwrite(outputFd, "\n", 1);

			/* Rewrite the prompt */
			xwrite(outputFd, prompt, strlen(prompt));

			/* Reset the command string */
			memset(command, 0, sizeof(command));
			len = cursor = 0;

			break;
		case 4:
			/* Control-d -- Delete one character, or exit 
			 * if the len=0 and no chars to delete */
			if (len == 0) {
				xwrite(outputFd, "exit", 4);
				clean_up_and_die(0);
			} else {
				input_delete(command, outputFd, cursor, &len);
			}
			break;
		case 5:
			/* Control-e -- End of line */
			input_end(outputFd, &cursor, len);
			break;
		case 6:
			/* Control-f -- Move forward one character */
			input_forward(outputFd, &cursor, len);
			break;
		case '\b':
		case DEL:
			/* Control-h and DEL */
			input_backspace(command, outputFd, &cursor, &len);
			break;
		case '\t':
#ifdef BB_FEATURE_SH_TAB_COMPLETION
			input_tab(command, prompt, outputFd, &cursor, &len);
#endif
			break;
		case 14:
			/* Control-n -- Get next command in history */
			if (hp && hp->n && hp->n->s) {
				get_next_history(&hp, command);
				goto rewrite_line;
			} else {
				xwrite(outputFd, "\007", 1);
			}
			break;
		case 16:
			/* Control-p -- Get previous command from history */
			if (hp && hp->p) {
				get_previous_history(&hp, command);
				goto rewrite_line;
			} else {
				xwrite(outputFd, "\007", 1);
			}
			break;
		case ESC:{
				/* escape sequence follows */
				if ((ret = read(inputFd, &c, 1)) < 1)
					return;

				if (c == '[') {	/* 91 */
					if ((ret = read(inputFd, &c, 1)) < 1)
						return;

					switch (c) {
					case 'A':
						/* Up Arrow -- Get previous command from history */
						if (hp && hp->p) {
							get_previous_history(&hp, command);
							goto rewrite_line;
						} else {
							xwrite(outputFd, "\007", 1);
						}
						break;
					case 'B':
						/* Down Arrow -- Get next command in history */
						if (hp && hp->n && hp->n->s) {
							get_next_history(&hp, command);
							goto rewrite_line;
						} else {
							xwrite(outputFd, "\007", 1);
						}
						break;

						/* Rewrite the line with the selected history item */
					  rewrite_line:
						/* erase old command from command line */
						len = strlen(command)-strlen(hp->s);
						while (len>0)
							input_backspace(command, outputFd, &cursor, &len);
						input_home(outputFd, &cursor);
						
						/* write new command */
						strcpy(command, hp->s);
						len = strlen(hp->s);
						xwrite(outputFd, command, len);
						cursor = len;
						break;
					case 'C':
						/* Right Arrow -- Move forward one character */
						input_forward(outputFd, &cursor, len);
						break;
					case 'D':
						/* Left Arrow -- Move back one character */
						input_backward(outputFd, &cursor);
						break;
					case '3':
						/* Delete */
						input_delete(command, outputFd, cursor, &len);
						break;
					case '1':
						/* Home (Ctrl-A) */
						input_home(outputFd, &cursor);
						break;
					case '4':
						/* End (Ctrl-E) */
						input_end(outputFd, &cursor, len);
						break;
					default:
						xwrite(outputFd, "\007", 1);
					}
					if (c == '1' || c == '3' || c == '4')
						if ((ret = read(inputFd, &c, 1)) < 1)
							return;	/* read 126 (~) */
				}
				if (c == 'O') {
					/* 79 */
					if ((ret = read(inputFd, &c, 1)) < 1)
						return;
					switch (c) {
					case 'H':
						/* Home (xterm) */
						input_home(outputFd, &cursor);
						break;
					case 'F':
						/* End (xterm) */
						input_end(outputFd, &cursor, len);
						break;
					default:
						xwrite(outputFd, "\007", 1);
					}
				}
				c = 0;
				break;
			}

		default:				/* If it's regular input, do the normal thing */

			if (!isprint(c)) {	/* Skip non-printable characters */
				break;
			}

			if (len >= (BUFSIZ - 2))	/* Need to leave space for enter */
				break;

			len++;

			if (cursor == (len - 1)) {	/* Append if at the end of the line */
				*(command + cursor) = c;
			} else {			/* Insert otherwise */
				memmove(command + cursor + 1, command + cursor,
						len - cursor - 1);

				*(command + cursor) = c;

				for (j = cursor; j < len; j++)
					xwrite(outputFd, command + j, 1);
				for (; j > cursor; j--)
					xwrite(outputFd, "\033[D", 3);
			}

			cursor++;
			xwrite(outputFd, &c, 1);
			break;
		}
		if (c == '\t')
			lastWasTab = TRUE;
		else
			lastWasTab = FALSE;

		if (break_out)			/* Enter is the command terminator, no more input. */
			break;
	}

	nr = len + 1;
	setTermSettings(inputFd, (void *) &initial_settings);
	reset_term = 0;


	/* Handle command history log */
	if (*(command)) {

		struct history *h = his_end;

		if (!h) {
			/* No previous history */
			h = his_front = malloc(sizeof(struct history));
			h->n = malloc(sizeof(struct history));

			h->p = NULL;
			h->s = strdup(command);
			h->n->p = h;
			h->n->n = NULL;
			h->n->s = NULL;
			his_end = h->n;
			history_counter++;
		} else {
			/* Add a new history command */
			h->n = malloc(sizeof(struct history));

			h->n->p = h;
			h->n->n = NULL;
			h->n->s = NULL;
			h->s = strdup(command);
			his_end = h->n;

			/* After max history, remove the oldest command */
			if (history_counter >= MAX_HISTORY) {

				struct history *p = his_front->n;

				p->p = NULL;
				free(his_front->s);
				free(his_front);
				his_front = p;
			} else {
				history_counter++;
			}
		}
	}

	return;
}

extern void cmdedit_init(void)
{
	atexit(cmdedit_reset_term);
	signal(SIGKILL, clean_up_and_die);
	signal(SIGINT, clean_up_and_die);
	signal(SIGQUIT, clean_up_and_die);
	signal(SIGTERM, clean_up_and_die);
}
#endif							/* BB_FEATURE_SH_COMMAND_EDITING */
