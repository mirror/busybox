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
   Ctrl-E also works as End.


   Editor with vertical scrolling and completion by
   Vladimir Oleynik. vodz@usa.net (c) 2001

   Small bug: not true work if terminal size (x*y symbols) less
	      size (prompt + editor`s line + 2 symbols)
 */



#include "busybox.h"

#ifdef BB_FEATURE_SH_COMMAND_EDITING

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <signal.h>

#ifdef BB_FEATURE_SH_TAB_COMPLETION
#include <sys/stat.h>
#endif

static const int MAX_HISTORY = 15;		/* Maximum length of the linked list for the command line history */

enum {
	ESC = 27,
	DEL = 127,
};

#define member(c, s) ((c) ? ((char *)strchr ((s), (c)) != (char *)NULL) : 0)
#define whitespace(c) (((c) == ' ') || ((c) == '\t'))

static struct history *his_front = NULL;	/* First element in command line list */
static struct history *his_end = NULL;	/* Last element in command line list */

/* ED: sparc termios is broken: revert back to old termio handling. */

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
static struct termios initial_settings, new_settings;


#ifndef	_POSIX_VDISABLE
#define	_POSIX_VDISABLE	'\0'
#endif


static
volatile int cmdedit_termw;       /* actual terminal width */
static   int history_counter = 0; /* Number of commands in history list */

static
volatile int handlers_sets = 0;    /* Set next bites
				     when atexit() has been called
				     and set many "terminates" signal handlers
				     and winchg signal handler
				     and if the terminal needs to be reset upon exit
				     */
enum {
	SET_ATEXIT        = 1,
	SET_TERM_HANDLERS = 2,
	SET_WCHG_HANDLERS = 4,
	SET_RESET_TERM    = 8,
};

	
static   int cmdedit_x;           /* real x terminal position,
				   require put prompt in start x position */
static   int cmdedit_y;           /* pseudoreal y terminal position */
static   int cmdedit_prmt_len;    /* for fast running, without duplicate calculate */

static   int cursor;              /* required global for signal handler */
static   int len;                 /* --- "" - - "" - -"- --""-- --""--- */
static  char *command_ps;         /* --- "" - - "" - -"- --""-- --""--- */
static const char *cmdedit_prompt;/* --- "" - - "" - -"- --""-- --""--- */

/* Link into lash to reset context to 0
 * on ^C and such */
extern unsigned int shell_context;


struct history {
	char *s;
	struct history *p;
	struct history *n;
};

static void cmdedit_setwidth(int w, int redraw_flg);

static void win_changed(int nsig)
{
	struct winsize win = { 0, 0, 0, 0 };
	static __sighandler_t previous_SIGWINCH_handler; /* for reset */

	/* emulate signal call if not called as a sig handler */
	if(nsig == -SIGWINCH || nsig == SIGWINCH) {
		ioctl(0, TIOCGWINSZ, &win);
		if (win.ws_col > 0) {
			cmdedit_setwidth( win.ws_col, nsig == SIGWINCH );
		} else {
			/* Default to 79 if their console doesn't want to share */
			cmdedit_setwidth( 79, nsig == SIGWINCH );
		}
	}
	
	/* Unix not all standart in recall signal */

	if(nsig == -SIGWINCH)                  /* save previous handler */
		previous_SIGWINCH_handler = signal(SIGWINCH, win_changed);
	else if(nsig == SIGWINCH)              /* signaled called handler */
		signal(SIGWINCH, win_changed);     /* set for next call */
	else                                   /* set previous handler */
		signal(SIGWINCH, previous_SIGWINCH_handler);   /* reset */
}

static void cmdedit_reset_term(void)
{
	if((handlers_sets & SET_RESET_TERM)!=0) {
		/* sparc and other have broken termios support: use old termio handling. */
		setTermSettings(fileno(stdin), (void*) &initial_settings);
		handlers_sets &= ~SET_RESET_TERM;
	}
	if((handlers_sets & SET_WCHG_HANDLERS)!=0) {
		/* reset SIGWINCH handler to previous (default) */
		win_changed(0);
		handlers_sets &= ~SET_WCHG_HANDLERS;
	}
	fflush(stdout);
#ifdef BB_FEATURE_CLEAN_UP
	if (his_front) {
		struct history *n;
		//while(his_front!=his_end) {
		while(his_front!=his_end) {
			n = his_front->n;
			free(his_front->s);
			free(his_front);
			his_front=n;
		}
	}
#endif
}



/* special for recount position for scroll and remove terminal margin effect */
static void cmdedit_set_out_char(int c, int next_char) {
	putchar(c);
	if(++cmdedit_x>=cmdedit_termw) {
		/* terminal is scrolled down */
		cmdedit_y++;
		cmdedit_x=0;

		if(!next_char)
			next_char = ' ';
		/* destroy "(auto)margin" */
		putchar(next_char);
		putchar('\b');
	}
	cursor++;
}

/* Move to end line. Bonus: rewrite line from cursor without use
   special control terminal strings, also saved size and speed! */
static void input_end (void) {
	while(cursor < len)
		cmdedit_set_out_char(command_ps[cursor], 0);
}

/* Go to the next line */
static void goto_new_line(void) {
	input_end();
	cmdedit_set_out_char('\n', 0);
}


static inline void out1str(const char *s) { fputs  (s, stdout); }
static inline void beep   (void)          { putchar('\007');    }

/* Go to HOME position */
static void input_home(void)
{
	while(cmdedit_y>0) {            /* up to start y */
		out1str("\033[A");
		cmdedit_y--;
	}
	putchar('\r');
	cursor = 0;
	out1str(cmdedit_prompt);
	cmdedit_x = cmdedit_prmt_len;

}

/* Move back one charactor */
static void input_backward(void) {
	if (cursor > 0) {
		cursor--;
		if(cmdedit_x!=0) {   /* no first position in terminal line */
			putchar('\b');
			cmdedit_x--;
			}
		 else {
			out1str("\033[A");      /* up */
			cmdedit_y--;

			/* to end in current terminal line */
			while(cmdedit_x<(cmdedit_termw-1)) {
				out1str("\033[C");
				cmdedit_x++;
				}
			}
	}
}

/* Delete the char in front of the cursor */
static void input_delete(void)
{
	int j = cursor;

	if (j == len)
		return;
	
	memmove (command_ps + j, command_ps + j + 1, BUFSIZ - j - 1);
	len--;
	input_end();                            /* rewtite new line */
	cmdedit_set_out_char(' ', 0);           /* destroy end char */
	while (j < cursor)
		input_backward();               /* back to old pos cursor */
}

/* Delete the char in back of the cursor */
static void input_backspace(void)
{
	if (cursor > 0) {
		input_backward();
		input_delete  ();
	}
}


/* Move forward one charactor */
static void input_forward(void)
{
    if (cursor < len)
	cmdedit_set_out_char(command_ps[cursor], command_ps[cursor + 1]);
}


static void clean_up_and_die(int sig)
{
	goto_new_line();
	if (sig!=SIGINT)
		exit(EXIT_SUCCESS);  /* cmdedit_reset_term() called in atexit */
	cmdedit_reset_term();
}

static void cmdedit_setwidth(int w, int redraw_flg)
{
	cmdedit_termw = cmdedit_prmt_len+2;
	if (w > cmdedit_termw) {

		cmdedit_termw = w;

		if(redraw_flg) {
			int sav_cursor = cursor;

			/* set variables for new terminal size */
			cmdedit_y = sav_cursor/w;
			cmdedit_x = sav_cursor-cmdedit_y*w;

			/* redraw */
			input_home();
			input_end();
			while(sav_cursor<cursor)
				input_backward();
		}
	} else {
		error_msg("\n*** Error: minimum screen width is %d\n", cmdedit_termw);
	}
}

extern void cmdedit_init(void)
{
	if((handlers_sets & SET_WCHG_HANDLERS)==0) {
		/* pretend we received a signal in order to set term size and sig handling */
		win_changed(-SIGWINCH);
		handlers_sets |= SET_WCHG_HANDLERS;
	}

	if((handlers_sets & SET_ATEXIT)==0) {
		atexit(cmdedit_reset_term);     /* be sure to do this only once */
		handlers_sets |= SET_ATEXIT;
	}
	if((handlers_sets & SET_TERM_HANDLERS)==0) {
		signal(SIGKILL, clean_up_and_die);
		signal(SIGINT,  clean_up_and_die);
		signal(SIGQUIT, clean_up_and_die);
		signal(SIGTERM, clean_up_and_die);
		handlers_sets |= SET_TERM_HANDLERS;
	}
}

#ifdef BB_FEATURE_SH_TAB_COMPLETION

#ifdef BB_FEATURE_USERNAME_COMPLETION
static char** username_tab_completion(char *ud, int *num_matches)
{
    static struct passwd *entry;
    int                   userlen;
	char **matches = (char **) NULL;
    char                 *temp;
    int                   nm = 0;

    setpwent ();
    userlen = strlen (ud + 1);

    while ((entry = getpwent ()) != NULL) {
	/* Null usernames should result in all users as possible completions. */
	if (!userlen || !strncmp (ud + 1, entry->pw_name, userlen)) {

		temp = xmalloc (3 + strlen (entry->pw_name));
		sprintf(temp, "~%s/", entry->pw_name);

		matches = xrealloc(matches, (nm+1)*sizeof(char *));
		matches[nm++] = temp;
	}
    }

    endpwent ();
    (*num_matches) = nm;
	return (matches);
}
#endif

enum {
	FIND_EXE_ONLY  = 0,
	FIND_DIR_ONLY  = 1,
	FIND_FILE_ONLY = 2,
};

#include <dirent.h>

static int path_parse(char ***p, int flags)
{
	int  npth;
	char *tmp;
	char *pth;

	if(flags!=FIND_EXE_ONLY || (pth=getenv("PATH"))==0) {
	/* if not setenv PATH variable, to search cur dir "." */
		(*p) = xmalloc(sizeof(char *));
		(*p)[0] = xstrdup(".");
		return 1;
	}

	tmp = pth;
	npth=0;

	for(;;) {
		npth++; /* count words is + 1 count ':' */
		tmp = strchr(tmp, ':');
		if(tmp)
			tmp++;
		else
			break;
	}

	*p = xmalloc(npth*sizeof(char *));

	tmp = pth;
	(*p)[0] = xstrdup(tmp);
	npth=1;                 /* count words is + 1 count ':' */

	for(;;) {
		tmp = strchr(tmp, ':');
		if(tmp) {
			(*p)[0][(tmp-pth)]=0; /* ':' -> '\0'*/
			tmp++;
		} else
			break;
		(*p)[npth++] = &(*p)[0][(tmp-pth)]; /* p[next]=p[0][&'\0'+1] */
	}

	return npth;
}

static char** exe_n_cwd_tab_completion(char* command, int *num_matches, int type)
{
	char *dirName;
	char             **matches = 0;
	DIR *dir;
	struct dirent *next;
	char               cmd   [BUFSIZ+4];
	char              *dirbuf;
	char               found [BUFSIZ+4];
	int                nm = *num_matches;
	struct stat        st;
	char             **paths;
	int                npaths;
	int                i;
	char               full_pth[BUFSIZ+4+PATH_MAX];


	strcpy(cmd, command); /* save for change (last '/' to '\0') */

	dirName = strrchr(cmd, '/');
	if(dirName==NULL) {
		/* no dir, if flags==EXE_ONLY - get paths, else "." */
		npaths = path_parse(&paths, type);
		if(npaths==0)
			return 0;
	} else {
		/* with dir */

		/* save dir */
		dirbuf = xstrdup(cmd);
		/* set only dirname */
		dirbuf[(dirName-cmd)+1]=0;

		/* strip dirname in cmd */
		strcpy(cmd, dirName+1);
			
		paths = xmalloc(sizeof(char*));
		paths[0] = dirbuf;
		npaths = 1;      /* only 1 dir */
	}

	for(i=0; i < npaths; i++) {

		dir = opendir(paths[i]);
	if (!dir) {
		/* Don't print an error, just shut up and return */
		return (matches);
	}
	while ((next = readdir(dir)) != NULL) {
			/* matched ? */
			if(strncmp(next->d_name, cmd, strlen(cmd)))
				continue;
			/* not see .name without .match */
			if(*next->d_name == '.' && *cmd != '.')
				continue;
			sprintf(full_pth, "%s/%s", paths[i], next->d_name);
			/* hmm, remover in progress? */
			if(stat(full_pth, &st)<0)
					continue;
			/* Cool, found a match. */
			if (S_ISDIR(st.st_mode)) {
				/* name is directory */
				strcpy(found, next->d_name);
				strcat(found, "/");
				if(type==FIND_DIR_ONLY)
					strcat(found, " ");
			} else {
			  /* not put found file if search only dirs for cd */
				if(type==FIND_DIR_ONLY)
			continue;
				strcpy(found, next->d_name);
				strcat(found, " ");
		} 
			/* Add it to the list */
			matches = xrealloc(matches, (nm+1)*sizeof(char *));
			matches[nm++] = xstrdup(found);
		}
	}
	free(paths[0]); /* allocate memory only in first member */
	free(paths);
	*num_matches = nm;
	return (matches);
}

static void input_tab(int lastWasTab)
{
	/* Do TAB completion */
	static int    num_matches;
	static char **matches;

	char          matchBuf[BUFSIZ];

	int           pos = cursor;
	int           find_type=FIND_FILE_ONLY;


	if (lastWasTab == FALSE) {
		char *tmp, *tmp1;
		int len_found;

		/* For now, we will not bother with trying to distinguish
		 * whether the cursor is in/at a command extression -- we
		 * will always try all possible matches.  If you don't like
		 * that then feel free to fix it.
		 */

		/* Make a local copy of the string -- up 
		 * to the position of the cursor */
		memset(matchBuf, 0, BUFSIZ);
		tmp = strncpy(matchBuf, command_ps, cursor);

		/* skip past any command seperator tokens */
		while ( (tmp1=strpbrk(tmp, ";|&{(`")) != NULL) {
			tmp = ++tmp1;
		}

		/* skip any leading white space */
		while (*tmp == ' ')
			tmp++;

		if(strncmp(tmp, "cd ", 3)==0)
			find_type = FIND_DIR_ONLY;
		 else if(strchr(tmp, ' ')==NULL)
			find_type = FIND_EXE_ONLY;

		/* find begin curent word */
		if( (tmp1=strrchr(tmp, ' ')) != NULL) {
			tmp = ++tmp1;
		}
		strcpy(matchBuf, tmp);

		/* Free up any memory already allocated */
		if (matches) {
			while(num_matches>0)
				free(matches[--num_matches]);
			free(matches);
			matches = (char **) NULL;
		}

#ifdef BB_FEATURE_USERNAME_COMPLETION
		/* If the word starts with `~' and there is no slash in the word, 
		 * then try completing this word as a username. */

		if (matchBuf[0]=='~' && strchr(matchBuf, '/')==0) {
			matches = username_tab_completion(matchBuf, &num_matches);
		}
#endif
		/* Try to match any executable in our path and everything 
		 * in the current working directory that matches.  */
		if (!matches)
			matches = exe_n_cwd_tab_completion(matchBuf, &num_matches, find_type);

		/* Did we find exactly one match? */
		if(!matches || num_matches>1) {
			beep();
			return;
		}

		len_found = strlen(matches[0]);

		/* have space to placed match? */
		if ( (len_found-strlen(matchBuf)+len) < BUFSIZ ) {

			int recalc_pos = len;

			/* before word for match */
			command_ps[pos-strlen(matchBuf)]=0;

			/* tail line */
			strcpy(matchBuf, command_ps+pos);

			/* add match */
			strcat(command_ps, matches[0]);
			/* add tail */
			strcat(command_ps, matchBuf);

			/* write out the matched command */
			len=strlen(command_ps);
			recalc_pos = len-recalc_pos+pos;
			input_end();    /* write */
			while(recalc_pos<cursor)
				input_backward();
			return;
		}
	} else {
		/* Ok -- the last char was a TAB.  Since they
		 * just hit TAB again, print a list of all the
		 * available choices... */
		if ( matches && num_matches>0 ) {
			int i, col;
			int sav_cursor = cursor;

			/* Go to the next line */
			goto_new_line();
			for (i=0,col=0; i<num_matches; i++) {
				printf("%s  ", matches[i]);
				col += strlen(matches[i])+2;
				col -= (col/cmdedit_termw)*cmdedit_termw;
				if (col > 60 && matches[i+1] != NULL) {
					putchar('\n');
					col = 0;
				}
			}
			/* Go to the next line and rewrite the prompt */
			printf("\n%s", cmdedit_prompt);
			cmdedit_x = cmdedit_prmt_len;
			cmdedit_y = 0;
			cursor    = 0;
			input_end();    /* Rewrite the command */
			/* Put the cursor back to where it used to be */
			while (sav_cursor < cursor)
				input_backward();
		}
	}
}
#endif

static void get_previous_history(struct history **hp, char* command)
{
	if ((*hp)->s)
		free((*hp)->s);
	(*hp)->s = strdup(command);
	*hp = (*hp)->p;
}

static void get_next_history(struct history **hp, char* command)
{
	if ((*hp)->s)
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

	int j = 0;
	int break_out = 0;
	int ret = 0;
	int lastWasTab = FALSE;
	char c = 0;
	struct history *hp = his_end;

	len = 0;
	cursor = 0;
	command_ps = command;

	if (new_settings.c_cc[VMIN]==0) {
		
		getTermSettings(inputFd, (void*) &initial_settings);
		memcpy(&new_settings, &initial_settings, sizeof(struct termios));
		new_settings.c_cc[VMIN] = 1;
		new_settings.c_cc[VTIME] = 0;
		new_settings.c_cc[VINTR] = _POSIX_VDISABLE; /* Turn off CTRL-C, so we can trap it */
		new_settings.c_lflag &= ~ICANON;	/* unbuffered input */
		new_settings.c_lflag &= ~(ECHO|ECHOCTL|ECHONL); /* Turn off echoing */
	}
	setTermSettings(inputFd, (void*) &new_settings);
	handlers_sets |= SET_RESET_TERM;

	memset(command, 0, BUFSIZ);

	cmdedit_init();

	/* Print out the command prompt */
	cmdedit_prompt = prompt;
	cmdedit_prmt_len = strlen(prompt);
	printf("%s", prompt);
	cmdedit_x = cmdedit_prmt_len;   /* count real x terminal position */
	cmdedit_y = 0;                  /* quasireal y, not true work if line > xt*yt */


	while (1) {

		fflush(stdout);         /* buffered out to fast */

		if ((ret = read(inputFd, &c, 1)) < 1)
			return;
		//fprintf(stderr, "got a '%c' (%d)\n", c, c);

		switch (c) {
		case '\n':
		case '\r':
			/* Enter */
			*(command + len) = c;
			len++;
			input_end ();
			break_out = 1;
			break;
		case 1:
			/* Control-a -- Beginning of line */
			input_home();
			break;
		case 2:
			/* Control-b -- Move back one character */
			input_backward();
			break;
		case 3:
			/* Control-c -- stop gathering input */
			
			/* Link into lash to reset context to 0 on ^C and such */
			shell_context = 0;

			/* Go to the next line */
			goto_new_line();

#if 0
			/* Rewrite the prompt */
			printf("%s", prompt);

			/* Reset the command string */
			memset(command, 0, BUFSIZ);
			len = cursor = 0;
#endif
			return;

		case 4:
			/* Control-d -- Delete one character, or exit 
			 * if the len=0 and no chars to delete */
			if (len == 0) {
				printf("exit");
				clean_up_and_die(0);
			} else {
				input_delete();
			}
			break;
		case 5:
			/* Control-e -- End of line */
			input_end();
			break;
		case 6:
			/* Control-f -- Move forward one character */
			input_forward();
			break;
		case '\b':
		case DEL:
			/* Control-h and DEL */
			input_backspace();
			break;
		case '\t':
#ifdef BB_FEATURE_SH_TAB_COMPLETION
			input_tab(lastWasTab);
#endif
			break;
		case 14:
			/* Control-n -- Get next command in history */
			if (hp && hp->n && hp->n->s) {
				get_next_history(&hp, command);
				goto rewrite_line;
			} else {
				beep();
			}
			break;
		case 16:
			/* Control-p -- Get previous command from history */
			if (hp && hp->p) {
				get_previous_history(&hp, command);
				goto rewrite_line;
			} else {
				beep();
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
							beep();
						}
						break;
					case 'B':
						/* Down Arrow -- Get next command in history */
						if (hp && hp->n && hp->n->s) {
							get_next_history(&hp, command);
							goto rewrite_line;
						} else {
							beep();
						}
						break;

						/* Rewrite the line with the selected history item */
					  rewrite_line:
						/* return to begin of line */
						input_home ();
						/* for next memmoves without set '\0' */
						memset (command, 0, BUFSIZ);
						/* change command */
						strcpy (command, hp->s);
						/* write new command */
						for (j=0; command[j]; j++)
							cmdedit_set_out_char(command[j], 0);
						ret = cursor;
						/* erase tail if required */
						for (j = ret; j < len; j++)
							cmdedit_set_out_char(' ', 0);
						/* and backward cursor */
						for (j = ret; j < len; j++)
							input_backward();
						len = cursor;                           /* set new len */
						break;
					case 'C':
						/* Right Arrow -- Move forward one character */
						input_forward();
						break;
					case 'D':
						/* Left Arrow -- Move back one character */
						input_backward();
						break;
					case '3':
						/* Delete */
						input_delete();
						break;
					case '1':
						/* Home (Ctrl-A) */
						input_home();
						break;
					case '4':
						/* End (Ctrl-E) */
						input_end();
						break;
					default:
						beep();
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
						input_home();
						break;
					case 'F':
						/* End (xterm) */
						input_end();
						break;
					default:
						beep();
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
				cmdedit_set_out_char(c, command[cursor+1]);
			} else {			/* Insert otherwise */
				memmove(command + cursor + 1, command + cursor,
						len - cursor - 1);

				*(command + cursor) = c;
				j = cursor+1;
				/* rewrite from cursor */
				input_end ();
				/* to prev x pos + 1 */
				while(cursor > j)
					input_backward();
			}

			break;
		}
		if (c == '\t')
			lastWasTab = TRUE;
		else
			lastWasTab = FALSE;

		if (break_out)			/* Enter is the command terminator, no more input. */
			break;
	}

	setTermSettings (inputFd, (void *) &initial_settings);
	handlers_sets &= ~SET_RESET_TERM;

	/* Handle command history log */
	if (len>1) {    /* no put empty line (only '\n') */

		struct history *h = his_end;
		char           *ss;

		command[len-1] = 0;     /* destroy end '\n' */
		ss = strdup(command);   /* duplicate without '\n' */
		command[len-1] = '\n';  /* restore '\n' */

		if (!h) {
			/* No previous history -- this memory is never freed */
			h = his_front = xmalloc(sizeof(struct history));
			h->n = xmalloc(sizeof(struct history));

			h->p = NULL;
			h->s = ss;
			h->n->p = h;
			h->n->n = NULL;
			h->n->s = NULL;
			his_end = h->n;
			history_counter++;
		} else {
			/* Add a new history command -- this memory is never freed */
			h->n = xmalloc(sizeof(struct history));

			h->n->p = h;
			h->n->n = NULL;
			h->n->s = NULL;
			h->s = ss;
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


/* Undo the effects of cmdedit_init(). */
extern void cmdedit_terminate(void)
{
	cmdedit_reset_term();
	if((handlers_sets & SET_TERM_HANDLERS)!=0) {
		signal(SIGKILL, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		signal(SIGWINCH, SIG_DFL);
		handlers_sets &= ~SET_TERM_HANDLERS;
	}
}



#endif	/* BB_FEATURE_SH_COMMAND_EDITING */
