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

#include "cmdedit.h"


#define  MAX_HISTORY   15	/* Maximum length of the linked list for the command line history */

#define ESC	27
#define DEL	127
#define member(c, s) ((c) ? ((char *)strchr ((s), (c)) != (char *)NULL) : 0)
#define whitespace(c) (((c) == ' ') || ((c) == '\t'))

static struct history *his_front = NULL;	/* First element in command line list */
static struct history *his_end = NULL;	/* Last element in command line list */
static struct termio old_term, new_term;	/* Current termio and the previous termio before starting ash */

static int history_counter = 0;	/* Number of commands in history list */
static int reset_term = 0;	/* Set to true if the terminal needs to be reset upon exit */
char *parsenextc;		/* copy of parsefile->nextc */

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
{				/* Command line input routines */
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

char **username_completion_matches( char* matchBuf)
{
    fprintf(stderr, "\nin username_completion_matches\n");
    return ( (char**) NULL);
}
char **command_completion_matches( char* matchBuf)
{
    fprintf(stderr, "\nin command_completion_matches\n");
    return ( (char**) NULL);
}
char **directory_completion_matches( char* matchBuf)
{
    fprintf(stderr, "\nin directory_completion_matches\n");
    return ( (char**) NULL);
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
 *
 */
extern int cmdedit_read_input(int inputFd, int outputFd,
			    char command[BUFSIZ])
{

    int nr = 0;
    int len = 0;
    int j = 0;
    int cursor = 0;
    int break_out = 0;
    int ret = 0;
    int lastWasTab = FALSE;
    char **matches = (char **)NULL;
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
		free( hp->s);
		hp->s = strdup(parsenextc);
		hp = hp->n;
		goto hop;
	    }
	    break;
	case 16:
	    /* Control-p -- Get previous command */
	    if (hp && hp->p) {	
		free( hp->s);
		hp->s = strdup(parsenextc);
		hp = hp->p;
		goto hop;
	    }
	    break;
	case '\t':
	    {
	    /* Do TAB completion */
		int in_command_position=0, ti=len-1;

		if (lastWasTab == FALSE) {
		    char *tmp;
		    char *matchBuf;

		    if (matches) {
			free(matches);
			matches = (char **)NULL;
		    }

		    matchBuf = (char *) calloc(BUFSIZ, sizeof(char));

		    /* Make a local copy of the string -- up 
		     * to the the position of the cursor */
		    strcpy( matchBuf, parsenextc); 
		    matchBuf[cursor+1] = '\0';

		    fprintf(stderr, "matchBuf='%s'\n", matchBuf);

		    /* skip leading white space */
		    tmp = matchBuf;
		    while (*tmp && isspace(*tmp)) {
			(tmp)++;
			ti++;
		    }

		    /* Determine if this is a command word or not */
		    //while ((ti > -1) && (whitespace (matchBuf[ti]))) {
//printf("\nti=%d\n", ti);
		//	ti--;
		 //   }
printf("\nti=%d\n", ti);

		    if (ti < 0) {
			  in_command_position++;
		    } else if (member(matchBuf[ti], ";|&{(`")) {
			int this_char, prev_char;
			in_command_position++;
			/* Handle the two character tokens `>&', `<&', and `>|'.
			 We are not in a command position after one of these. */
			this_char = matchBuf[ti];
			prev_char = matchBuf[ti - 1];

			if ((this_char == '&' && (prev_char == '<' || prev_char == '>')) ||
				(this_char == '|' && prev_char == '>')) {
			    in_command_position = 0;
			} 
			/* For now, do not bother with catching quoted
			 * expressions and marking them as not in command
			 * positions.  Some other day.  Or not.
			 */
			//else if (char_is_quoted (matchBuf, ti)) {
			//    in_command_position = 0;
			//}
		    }
printf("\nin_command_position=%d\n", in_command_position);
		    /* If the word starts in `~', and there is no slash in the word, 
		     * then try completing this word as a username. */
		    if (*matchBuf == '~' && !strchr (matchBuf, '/'))
			matches = username_completion_matches(matchBuf);

		    /* If this word is in a command position, then complete over possible 
		     * command names, including aliases, built-ins, and executables. */
		    if (!matches && in_command_position) {
			matches = command_completion_matches(matchBuf);

		    /* If we are attempting command completion and nothing matches, 
		     * then try and match directories as a last resort... */
		    if (!matches)
			matches = directory_completion_matches(matchBuf);
		    }
		} else {
		    printf("\nprinting match list\n");
		}
		/* Rewrite the whole line (for debugging) */
		for (; cursor > 0; cursor--)	
		    xwrite(outputFd, "\b", 1);
		len = strlen(parsenextc);
		xwrite(outputFd, parsenextc, len);
		cursor = len;
		break;
	    }
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
	case ESC:		{
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
			free( hp->s);
			hp->s = strdup(parsenextc);
			hp = hp->p;
			goto hop;
		    }
		    break;
		case 'B':
		    /* Down Arrow -- Get next command */
		    if (hp && hp->n && hp->n->s) {	
			free( hp->s);
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
		    xwrite(outputFd, parsenextc, len);

		    /* erase old command */
		    for (j = 0; j < len; j++)	
			xwrite(outputFd, " ", 1);

		    /* return to begining of line */
		    for (j = len; j > 0; j--)	
			xwrite(outputFd, "\b", 1);

		    memset(parsenextc, 0, BUFSIZ);
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
		case '1':	
		    /* Home (Ctrl-A) */
		    input_home(outputFd, &cursor);
		    break;
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

	default:		/* If it's regular input, do the normal thing */

	    if (!isprint(c))	/* Skip non-printable characters */
		break;

	    if (len >= (BUFSIZ - 2))	/* Need to leave space for enter */
		break;

	    len++;

	    if (cursor == (len - 1)) {	/* Append if at the end of the line */
		*(parsenextc + cursor) = c;
	    } else {		/* Insert otherwise */
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
	if (c=='\t')
	    lastWasTab = TRUE;
	else
	    lastWasTab = FALSE;

	if (break_out)		/* Enter is the command terminator, no more input. */
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
#endif				/* BB_FEATURE_SH_COMMAND_EDITING */
