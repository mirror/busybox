/* vi: set sw=4 ts=4: */
/*
 * Termios command line History and Editting for NetBSD sh (ash)
 * Copyright (c) 1999
 *      Main code:            Adam Rogoyski <rogoyski@cs.utexas.edu> 
 *      Etc:                  Dave Cinege <dcinege@psychosis.com>
 *      Adjusted for busybox: Erik Andersen <andersee@debian.org>
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
#include <termio.h>
#include <ctype.h>
#include <signal.h>


#define  MAX_HISTORY   15		/* Maximum length of the linked list for the command line history */

#define ESC	27
#define DEL	127
#define member(c, s) ((c) ? ((char *)strchr ((s), (c)) != (char *)NULL) : 0)
#define whitespace(c) (((c) == ' ') || ((c) == '\t'))

static struct history *his_front = NULL;	/* First element in command line list */
static struct history *his_end = NULL;	/* Last element in command line list */
static struct termio old_term, new_term;	/* Current termio and the previous termio before starting ash */

static int history_counter = 0;	/* Number of commands in history list */
static int reset_term = 0;		/* Set to true if the terminal needs to be reset upon exit */
char *parsenextc;				/* copy of parsefile->nextc */

struct history {
	char *s;
	struct history *p;
	struct history *n;
};


/* Version of write which resumes after a signal is caught.  */
int xwrite(int fd, char *buf, int nbytes)
{
	int ntry;
	int i;
	int n;

	n = nbytes;
	ntry = 0;
	for (;;) {
		i = write(fd, buf, n);
		if (i > 0) {
			if ((n -= i) <= 0)
				return nbytes;
			buf += i;
			ntry = 0;
		} else if (i == 0) {
			if (++ntry > 10)
				return nbytes - n;
		} else if (errno != EINTR) {
			return -1;
		}
	}
}


/* Version of ioctl that retries after a signal is caught.  */
int xioctl(int fd, unsigned long request, char *arg)
{
	int i;

	while ((i = ioctl(fd, request, arg)) == -1 && errno == EINTR);
	return i;
}


void cmdedit_reset_term(void)
{
	if (reset_term)
		xioctl(fileno(stdin), TCSETA, (void *) &old_term);
}

void prepareToDie(int sig)
{
	cmdedit_reset_term();
	fprintf(stdout, "\n");
	exit(TRUE);
}

void input_home(int outputFd, int *cursor)
{								/* Command line input routines */
	while (*cursor > 0) {
		xwrite(outputFd, "\b", 1);
		--*cursor;
	}
}


void input_delete(int outputFd, int cursor)
{
	int j = 0;

	memmove(parsenextc + cursor, parsenextc + cursor + 1,
			BUFSIZ - cursor - 1);
	for (j = cursor; j < (BUFSIZ - 1); j++) {
		if (!*(parsenextc + j))
			break;
		else
			xwrite(outputFd, (parsenextc + j), 1);
	}

	xwrite(outputFd, " \b", 2);

	while (j-- > cursor)
		xwrite(outputFd, "\b", 1);
}


void input_end(int outputFd, int *cursor, int len)
{
	while (*cursor < len) {
		xwrite(outputFd, "\033[C", 3);
		++*cursor;
	}
}


void input_backspace(int outputFd, int *cursor, int *len)
{
	int j = 0;

	if (*cursor > 0) {
		xwrite(outputFd, "\b \b", 3);
		--*cursor;
		memmove(parsenextc + *cursor, parsenextc + *cursor + 1,
				BUFSIZ - *cursor + 1);

		for (j = *cursor; j < (BUFSIZ - 1); j++) {
			if (!*(parsenextc + j))
				break;
			else
				xwrite(outputFd, (parsenextc + j), 1);
		}

		xwrite(outputFd, " \b", 2);

		while (j-- > *cursor)
			xwrite(outputFd, "\b", 1);

		--*len;
	}
}

#ifdef BB_FEATURE_SH_TAB_COMPLETION

char** username_completion_matches(char* command, int *num_matches)
{
	char **matches = (char **) NULL;
	*num_matches=0;
	fprintf(stderr, "\nin username_completion_matches\n");
	return (matches);
}

#include <dirent.h>
char** find_path_executable_n_cwd_matches(char* command, int *num_matches)
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
#endif

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
 *
 */
extern int cmdedit_read_input(char* prompt, int inputFd, int outputFd,
							  char command[BUFSIZ])
{

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
	parsenextc = command;
	if (!reset_term) {
		xioctl(inputFd, TCGETA, (void *) &old_term);
		memcpy(&new_term, &old_term, sizeof(struct termio));

		new_term.c_cc[VMIN] = 1;
		new_term.c_cc[VTIME] = 0;
		new_term.c_lflag &= ~ICANON;	/* unbuffered input */
		new_term.c_lflag &= ~ECHO;
		xioctl(inputFd, TCSETA, (void *) &new_term);
		reset_term = 1;
	} else {
		xioctl(inputFd, TCSETA, (void *) &new_term);
	}

	memset(parsenextc, 0, BUFSIZ);

	while (1) {

		if ((ret = read(inputFd, &c, 1)) < 1)
			return ret;

		fprintf(stderr, "\n\nkey=%d (%c)\n\n", c, c);
		/* Go to the next line */
		xwrite(outputFd, "\n", 1);
		/* Rewrite the prompt */
		xwrite(outputFd, prompt, strlen(prompt));
		/* Rewrite the command */
		xwrite(outputFd, parsenextc, len);

		switch (c) {
		case 1:
			/* Control-a -- Beginning of line */
			input_home(outputFd, &cursor);
		case 5:
			/* Control-e -- End of line */
			input_end(outputFd, &cursor, len);
			break;
		case 2:
			/* Control-b -- Move back one character */
			if (cursor > 0) {
				xwrite(outputFd, "\033[D", 3);
				cursor--;
			}
			break;
		case 6:
			/* Control-f -- Move forward one character */
			if (cursor < len) {
				xwrite(outputFd, "\033[C", 3);
				cursor++;
			}
			break;
		case 4:
			/* Control-d -- Delete one character */
			if (cursor != len) {
				input_delete(outputFd, cursor);
				len--;
			} else if (len == 0) {
				prepareToDie(0);
				exit(0);
			}
			break;
		case 14:
			/* Control-n -- Get next command */
			if (hp && hp->n && hp->n->s) {
				free(hp->s);
				hp->s = strdup(parsenextc);
				hp = hp->n;
				goto hop;
			}
			break;
		case 16:
			/* Control-p -- Get previous command */
			if (hp && hp->p) {
				free(hp->s);
				hp->s = strdup(parsenextc);
				hp = hp->p;
				goto hop;
			}
			break;
		case '\t':
#ifdef BB_FEATURE_SH_TAB_COMPLETION
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
					strncpy(matchBuf, parsenextc, cursor);
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
						matches = username_completion_matches(tmp, &num_matches);

					/* Try to match any executable in our path and everything 
					 * in the current working directory that matches.  */
					if (!matches)
						matches = find_path_executable_n_cwd_matches(tmp, &num_matches);

					/* Don't leak memory */
					free( matchBuf);

					/* Did we find exactly one match? */
					if (matches && num_matches==1) {
						/* write out the matched command */
						strncpy(parsenextc+pos, matches[0]+pos, strlen(matches[0])-pos);
						len=strlen(parsenextc);
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
						xwrite(outputFd, parsenextc, len);
						/* Put the cursor back to where it used to be */
						for (cursor=len; cursor > pos; cursor--)
							xwrite(outputFd, "\b", 1);
					}
				}
				break;
			}
#else
			break;
#endif
		case '\b':
		case DEL:
			/* Backspace */
			input_backspace(outputFd, &cursor, &len);
			break;
		case '\n':
			/* Enter */
			*(parsenextc + len++ + 1) = c;
			xwrite(outputFd, &c, 1);
			break_out = 1;
			break;
		case ESC:{
				/* escape sequence follows */
				if ((ret = read(inputFd, &c, 1)) < 1)
					return ret;

				if (c == '[') {	/* 91 */
					if ((ret = read(inputFd, &c, 1)) < 1)
						return ret;

					switch (c) {
					case 'A':
						/* Up Arrow -- Get previous command */
						if (hp && hp->p) {
							free(hp->s);
							hp->s = strdup(parsenextc);
							hp = hp->p;
							goto hop;
						}
						break;
					case 'B':
						/* Down Arrow -- Get next command */
						if (hp && hp->n && hp->n->s) {
							free(hp->s);
							hp->s = strdup(parsenextc);
							hp = hp->n;
							goto hop;
						}
						break;

						/* This is where we rewrite the line 
						 * using the selected history item */
					  hop:
						len = strlen(parsenextc);

						/* return to begining of line */
						for (; cursor > 0; cursor--)
							xwrite(outputFd, "\b", 1);

						/* erase old command */
						for (j = 0; j < len; j++)
							xwrite(outputFd, " ", 1);

						/* return to begining of line */
						for (j = len; j > 0; j--)
							xwrite(outputFd, "\b", 1);

						memset(parsenextc, 0, BUFSIZ);
						len = strlen(parsenextc);
						/* write new command */
						strcpy(parsenextc, hp->s);
						len = strlen(hp->s);
						xwrite(outputFd, parsenextc, len);
						cursor = len;
						break;
					case 'C':
						/* Right Arrow -- Move forward one character */
						if (cursor < len) {
							xwrite(outputFd, "\033[C", 3);
							cursor++;
						}
						break;
					case 'D':
						/* Left Arrow -- Move back one character */
						if (cursor > 0) {
							xwrite(outputFd, "\033[D", 3);
							cursor--;
						}
						break;
					case '3':
						/* Delete */
						if (cursor != len) {
							input_delete(outputFd, cursor);
							len--;
						}
						break;

					//case '5': case '6': /* pgup/pgdown */

					case '7':
						/* rxvt home */
					case '1':
						/* Home (Ctrl-A) */
						input_home(outputFd, &cursor);
						break;
					case '8':
						/* rxvt END */
					case '4':
						/* End (Ctrl-E) */
						input_end(outputFd, &cursor, len);
						break;
					}
					if (c == '1' || c == '3' || c == '4')
						if ((ret = read(inputFd, &c, 1)) < 1)
							return ret;	/* read 126 (~) */
				}
				if (c == 'O') {
					/* 79 */
					if ((ret = read(inputFd, &c, 1)) < 1)
						return ret;
					switch (c) {
					case 'H':
						/* Home (xterm) */
						input_home(outputFd, &cursor);
						break;
					case 'F':
						/* End (xterm) */
						input_end(outputFd, &cursor, len);
						break;
					}
				}
				c = 0;
				break;
			}

		default:				/* If it's regular input, do the normal thing */

			if (!isprint(c))	/* Skip non-printable characters */
				break;

			if (len >= (BUFSIZ - 2))	/* Need to leave space for enter */
				break;

			len++;

			if (cursor == (len - 1)) {	/* Append if at the end of the line */
				*(parsenextc + cursor) = c;
			} else {			/* Insert otherwise */
				memmove(parsenextc + cursor + 1, parsenextc + cursor,
						len - cursor - 1);

				*(parsenextc + cursor) = c;

				for (j = cursor; j < len; j++)
					xwrite(outputFd, parsenextc + j, 1);
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
	xioctl(inputFd, TCSETA, (void *) &old_term);
	reset_term = 0;


	/* Handle command history log */
	if (*(parsenextc)) {

		struct history *h = his_end;

		if (!h) {
			/* No previous history */
			h = his_front = malloc(sizeof(struct history));
			h->n = malloc(sizeof(struct history));

			h->p = NULL;
			h->s = strdup(parsenextc);
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
			h->s = strdup(parsenextc);
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

	return nr;
}

extern void cmdedit_init(void)
{
	atexit(cmdedit_reset_term);
	signal(SIGINT, prepareToDie);
	signal(SIGQUIT, prepareToDie);
	signal(SIGTERM, prepareToDie);
}
#endif							/* BB_FEATURE_SH_COMMAND_EDITING */
