/* vi: set sw=4 ts=4: */
/*
 * Termios command line History and Editting.
 *
 * Copyright (c) 1986-2001 may safely be consumed by a BSD or GPL license.
 * Written by:   Vladimir Oleynik <vodz@usa.net>
 *
 * Used ideas:
 *      Adam Rogoyski    <rogoyski@cs.utexas.edu>
 *      Dave Cinege      <dcinege@psychosis.com>
 *      Jakub Jelinek (c) 1995
 *      Erik Andersen    <andersee@debian.org> (Majorly adjusted for busybox)
 *
 * This code is 'as is' with no warranty.
 *
 *
 */

/*
   Usage and Known bugs:
   Terminal key codes are not extensive, and more will probably
   need to be added. This version was created on Debian GNU/Linux 2.x.
   Delete, Backspace, Home, End, and the arrow keys were tested
   to work in an Xterm and console. Ctrl-A also works as Home.
   Ctrl-E also works as End.

   Small bugs (simple effect):
   - not true viewing if terminal size (x*y symbols) less
     size (prompt + editor`s line + 2 symbols)
 */


//#define TEST

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <signal.h>
#include <limits.h>

#ifndef TEST

#include "busybox.h"

#define D(x)

#else

#define BB_FEATURE_SH_COMMAND_EDITING
#define BB_FEATURE_SH_TAB_COMPLETION
#define BB_FEATURE_SH_USERNAME_COMPLETION
#define BB_FEATURE_NONPRINTABLE_INVERSE_PUT
#define BB_FEATURE_BASH_STYLE_PROMT
#define BB_FEATURE_CLEAN_UP

#define D(x)  x

#endif							/* TEST */

#ifdef BB_FEATURE_SH_TAB_COMPLETION
#include <dirent.h>
#include <sys/stat.h>
#endif

#ifdef BB_FEATURE_SH_COMMAND_EDITING

#ifndef BB_FEATURE_SH_TAB_COMPLETION
#undef  BB_FEATURE_SH_USERNAME_COMPLETION
#endif

#if defined(BB_FEATURE_SH_USERNAME_COMPLETION) || defined(BB_FEATURE_BASH_STYLE_PROMT)
#define BB_FEATURE_GETUSERNAME_AND_HOMEDIR
#endif

#ifdef BB_FEATURE_GETUSERNAME_AND_HOMEDIR
#ifndef TEST
#include "pwd_grp/pwd.h"
#else
#include <pwd.h>
#endif							/* TEST */
#endif							/* advanced FEATURES */


#ifdef TEST
void *xrealloc(void *old, size_t size)
{
	return realloc(old, size);
}

void *xmalloc(size_t size)
{
	return malloc(size);
}
char *xstrdup(const char *s)
{
	return strdup(s);
}

void *xcalloc(size_t size, size_t se)
{
	return calloc(size, se);
}

#define error_msg(s, d)                   fprintf(stderr, s, d)
#endif


struct history {
	char *s;
	struct history *p;
	struct history *n;
};

/* Maximum length of the linked list for the command line history */
static const int MAX_HISTORY = 15;

/* First element in command line list */
static struct history *his_front = NULL;

/* Last element in command line list */
static struct history *his_end = NULL;


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


#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE '\0'
#endif


static
volatile int cmdedit_termw = 80;	/* actual terminal width */
static int history_counter = 0;	/* Number of commands in history list */
static
volatile int handlers_sets = 0;	/* Set next bites: */

enum {
	SET_ATEXIT = 1,				/* when atexit() has been called and
								   get euid,uid,gid to fast compare  */
	SET_TERM_HANDLERS = 2,		/* set many terminates signal handlers */
	SET_WCHG_HANDLERS = 4,		/* winchg signal handler */
	SET_RESET_TERM = 8,			/* if the terminal needs to be reset upon exit */
};


static int cmdedit_x;			/* real x terminal position */
static int cmdedit_y;			/* pseudoreal y terminal position */
static int cmdedit_prmt_len;	/* lenght prompt without colores string */

static int cursor;				/* required global for signal handler */
static int len;					/* --- "" - - "" - -"- --""-- --""--- */
static char *command_ps;		/* --- "" - - "" - -"- --""-- --""--- */
static
#ifndef BB_FEATURE_BASH_STYLE_PROMT
	const
#endif
char *cmdedit_prompt;			/* --- "" - - "" - -"- --""-- --""--- */

/* Link into lash to reset context to 0 on ^C and such */
extern unsigned int shell_context;


#ifdef BB_FEATURE_GETUSERNAME_AND_HOMEDIR
static char *user_buf = "";
static char *home_pwd_buf = "";
static int my_euid;
#endif

#ifdef BB_FEATURE_BASH_STYLE_PROMT
static char *hostname_buf = "";
static int num_ok_lines = 1;
#endif


#ifdef  BB_FEATURE_SH_TAB_COMPLETION

#ifndef BB_FEATURE_GETUSERNAME_AND_HOMEDIR
static int my_euid;
#endif

static int my_uid;
static int my_gid;

#endif							/* BB_FEATURE_SH_TAB_COMPLETION */


static void cmdedit_setwidth(int w, int redraw_flg);

static void win_changed(int nsig)
{
	struct winsize win = { 0, 0, 0, 0 };
	static __sighandler_t previous_SIGWINCH_handler;	/* for reset */

	/*   emulate      || signal call */
	if (nsig == -SIGWINCH || nsig == SIGWINCH) {
		ioctl(0, TIOCGWINSZ, &win);
		if (win.ws_col > 0) {
			cmdedit_setwidth(win.ws_col, nsig == SIGWINCH);
		} 
	}
	/* Unix not all standart in recall signal */

	if (nsig == -SIGWINCH)		/* save previous handler   */
		previous_SIGWINCH_handler = signal(SIGWINCH, win_changed);
	else if (nsig == SIGWINCH)	/* signaled called handler */
		signal(SIGWINCH, win_changed);	/* set for next call       */
	else						/* nsig == 0 */
		/* set previous handler    */
		signal(SIGWINCH, previous_SIGWINCH_handler);	/* reset    */
}

static void cmdedit_reset_term(void)
{
	if ((handlers_sets & SET_RESET_TERM) != 0) {
		/* sparc and other have broken termios support: use old termio handling. */
		setTermSettings(fileno(stdin), (void *) &initial_settings);
		handlers_sets &= ~SET_RESET_TERM;
	}
	if ((handlers_sets & SET_WCHG_HANDLERS) != 0) {
		/* reset SIGWINCH handler to previous (default) */
		win_changed(0);
		handlers_sets &= ~SET_WCHG_HANDLERS;
	}
	fflush(stdout);
#ifdef BB_FEATURE_CLEAN_UP
	if (his_front) {
		struct history *n;

		//while(his_front!=his_end) {
		while (his_front != his_end) {
			n = his_front->n;
			free(his_front->s);
			free(his_front);
			his_front = n;
		}
	}
#endif
}


/* special for recount position for scroll and remove terminal margin effect */
static void cmdedit_set_out_char(int next_char)
{

	int c = command_ps[cursor];

	if (c == 0)
		c = ' ';				/* destroy end char? */
#ifdef BB_FEATURE_NONPRINTABLE_INVERSE_PUT
	if (!isprint(c)) {			/* Inverse put non-printable characters */
		if (((unsigned char) c) >= 128)
			c -= 128;
		if (((unsigned char) c) < ' ')
			c += '@';
		if (c == 127)
			c = '?';
		printf("\033[7m%c\033[0m", c);
	} else
#endif
		putchar(c);
	if (++cmdedit_x >= cmdedit_termw) {
		/* terminal is scrolled down */
		cmdedit_y++;
		cmdedit_x = 0;

		if (!next_char)
			next_char = ' ';
		/* destroy "(auto)margin" */
		putchar(next_char);
		putchar('\b');
	}
	cursor++;
}

/* Move to end line. Bonus: rewrite line from cursor */
static void input_end(void)
{
	while (cursor < len)
		cmdedit_set_out_char(0);
}

/* Go to the next line */
static void goto_new_line(void)
{
	input_end();
	if (cmdedit_x)
		putchar('\n');
}


static inline void out1str(const char *s)
{
	fputs(s, stdout);
}
static inline void beep(void)
{
	putchar('\007');
}

/* Move back one charactor */
/* special for slow terminal */
static void input_backward(int num)
{
	if (num > cursor)
		num = cursor;
	cursor -= num;				/* new cursor (in command, not terminal) */

	if (cmdedit_x >= num) {		/* no to up line */
		cmdedit_x -= num;
		if (num < 4)
			while (num-- > 0)
				putchar('\b');

		else
			printf("\033[%dD", num);
	} else {
		int count_y;

		if (cmdedit_x) {
			putchar('\r');		/* back to first terminal pos.  */
			num -= cmdedit_x;	/* set previous backward        */
		}
		count_y = 1 + num / cmdedit_termw;
		printf("\033[%dA", count_y);
		cmdedit_y -= count_y;
		/*  require  forward  after  uping   */
		cmdedit_x = cmdedit_termw * count_y - num;
		printf("\033[%dC", cmdedit_x);	/* set term cursor   */
	}
}

static void put_prompt(void)
{
	out1str(cmdedit_prompt);
	cmdedit_x = cmdedit_prmt_len;	/* count real x terminal position */
	cursor = 0;
}

#ifdef BB_FEATURE_BASH_STYLE_PROMT
static void
add_to_prompt(char **prmt_mem_ptr, int *alm, int *prmt_len,
			  const char *addb)
{
	int l = strlen(addb);

	*prmt_len += l;
	if (*alm < (*prmt_len) + 1) {
		*alm = (*prmt_len) + 1;
		*prmt_mem_ptr = xrealloc(*prmt_mem_ptr, *alm);
	}
	strcat(*prmt_mem_ptr, addb);
}
#endif

static void parse_prompt(const char *prmt_ptr)
{
#ifdef BB_FEATURE_BASH_STYLE_PROMT
	int alm = strlen(prmt_ptr) + 1;	/* supposedly require memory */
	int prmt_len = 0;
	int sub_len = 0;
	int flg_not_length = '[';
	char *prmt_mem_ptr = xstrdup(prmt_ptr);
	char pwd_buf[PATH_MAX + 1];
	char buf[16];
	int c;

	pwd_buf[0] = 0;
	*prmt_mem_ptr = 0;

	while (*prmt_ptr) {
		c = *prmt_ptr++;
		if (c == '\\') {
			c = *prmt_ptr;
			if (c == 0)
				break;
			prmt_ptr++;
			switch (c) {
			case 'u':
				add_to_prompt(&prmt_mem_ptr, &alm, &prmt_len, user_buf);
				continue;
			case 'h':
				if (hostname_buf[0] == 0) {
					hostname_buf = xcalloc(256, 1);
					if (gethostname(hostname_buf, 255) < 0) {
						strcpy(hostname_buf, "?");
					} else {
						char *s = strchr(hostname_buf, '.');

						if (s)
							*s = 0;
					}
				}
				add_to_prompt(&prmt_mem_ptr, &alm, &prmt_len,
							  hostname_buf);
				continue;
			case '$':
				c = my_euid == 0 ? '#' : '$';
				break;
			case 'w':
				if (pwd_buf[0] == 0) {
					int l;

					getcwd(pwd_buf, PATH_MAX);
					l = strlen(home_pwd_buf);
					if (home_pwd_buf[0] != 0 &&
						strncmp(home_pwd_buf, pwd_buf, l) == 0) {
						strcpy(pwd_buf + 1, pwd_buf + l);
						pwd_buf[0] = '~';
					}
				}
				add_to_prompt(&prmt_mem_ptr, &alm, &prmt_len, pwd_buf);
				continue;
			case '!':
				snprintf(buf, sizeof(buf), "%d", num_ok_lines);
				add_to_prompt(&prmt_mem_ptr, &alm, &prmt_len, buf);
				continue;
			case 'e':
			case 'E':			/* \e \E = \033 */
				c = '\033';
				break;
			case 'x':
			case 'X':
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':{
				int l;
				int ho = 0;
				char *eho;

				if (c == 'X')
					c = 'x';

				for (l = 0; l < 3;) {

					buf[l++] = *prmt_ptr;
					buf[l] = 0;
					ho = strtol(buf, &eho, c == 'x' ? 16 : 8);
					if (ho > UCHAR_MAX || (eho - buf) < l) {
						l--;
						break;
					}
					prmt_ptr++;
				}
				buf[l] = 0;
				ho = strtol(buf, 0, c == 'x' ? 16 : 8);
				c = ho == 0 ? '?' : (char) ho;
				break;
			}
			case '[':
			case ']':
				if (c == flg_not_length) {
					flg_not_length = flg_not_length == '[' ? ']' : '[';
					continue;
				}
				break;
			}
		}
		buf[0] = c;
		buf[1] = 0;
		add_to_prompt(&prmt_mem_ptr, &alm, &prmt_len, buf);
		if (flg_not_length == ']')
			sub_len++;
	}
	cmdedit_prmt_len = prmt_len - sub_len;
	cmdedit_prompt = prmt_mem_ptr;
#else
	cmdedit_prompt = prmt_ptr;
	cmdedit_prmt_len = strlen(prmt_ptr);
#endif
	put_prompt();
}


/* draw promt, editor line, and clear tail */
static void redraw(int y, int back_cursor)
{
	if (y > 0)					/* up to start y */
		printf("\033[%dA", y);
	cmdedit_y = 0;				/* new quasireal y */
	putchar('\r');
	put_prompt();
	input_end();				/* rewrite */
	printf("\033[J");			/* destroy tail after cursor */
	input_backward(back_cursor);
}

/* Delete the char in front of the cursor */
static void input_delete(void)
{
	int j = cursor;

	if (j == len)
		return;

	strcpy(command_ps + j, command_ps + j + 1);
	len--;
	input_end();				/* rewtite new line */
	cmdedit_set_out_char(0);	/* destroy end char */
	input_backward(cursor - j);	/* back to old pos cursor */
}

/* Delete the char in back of the cursor */
static void input_backspace(void)
{
	if (cursor > 0) {
		input_backward(1);
		input_delete();
	}
}


/* Move forward one charactor */
static void input_forward(void)
{
	if (cursor < len)
		cmdedit_set_out_char(command_ps[cursor + 1]);
}


static void clean_up_and_die(int sig)
{
	goto_new_line();
	if (sig != SIGINT)
		exit(EXIT_SUCCESS);		/* cmdedit_reset_term() called in atexit */
	cmdedit_reset_term();
}

static void cmdedit_setwidth(int w, int redraw_flg)
{
	cmdedit_termw = cmdedit_prmt_len + 2;
	if (w <= cmdedit_termw) {
		cmdedit_termw = cmdedit_termw % w;
	}
	if (w > cmdedit_termw) {

		cmdedit_termw = w;

		if (redraw_flg) {
			/* new y for current cursor */
			int new_y = (cursor + cmdedit_prmt_len) / w;

			/* redraw */
			redraw((new_y >= cmdedit_y ? new_y : cmdedit_y), len - cursor);
			fflush(stdout);
		}
	} 
}

extern void cmdedit_init(void)
{
	if ((handlers_sets & SET_WCHG_HANDLERS) == 0) {
		/* emulate usage handler to set handler and call yours work */
		win_changed(-SIGWINCH);
		handlers_sets |= SET_WCHG_HANDLERS;
	}

	if ((handlers_sets & SET_ATEXIT) == 0) {
#ifdef BB_FEATURE_GETUSERNAME_AND_HOMEDIR
		struct passwd *entry;

		my_euid = geteuid();
		entry = getpwuid(my_euid);
		if (entry) {
			user_buf = xstrdup(entry->pw_name);
			home_pwd_buf = xstrdup(entry->pw_dir);
		}
#endif

#ifdef  BB_FEATURE_SH_TAB_COMPLETION

#ifndef BB_FEATURE_GETUSERNAME_AND_HOMEDIR
		my_euid = geteuid();
#endif
		my_uid = getuid();
		my_gid = getgid();
#endif							/* BB_FEATURE_SH_TAB_COMPLETION */
		handlers_sets |= SET_ATEXIT;
		atexit(cmdedit_reset_term);	/* be sure to do this only once */
	}

	if ((handlers_sets & SET_TERM_HANDLERS) == 0) {
		signal(SIGKILL, clean_up_and_die);
		signal(SIGINT, clean_up_and_die);
		signal(SIGQUIT, clean_up_and_die);
		signal(SIGTERM, clean_up_and_die);
		handlers_sets |= SET_TERM_HANDLERS;
	}

}

#ifdef BB_FEATURE_SH_TAB_COMPLETION

static int is_execute(const struct stat *st)
{
	if ((!my_euid && (st->st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))) ||
		(my_uid == st->st_uid && (st->st_mode & S_IXUSR)) ||
		(my_gid == st->st_gid && (st->st_mode & S_IXGRP)) ||
		(st->st_mode & S_IXOTH)) return TRUE;
	return FALSE;
}

#ifdef BB_FEATURE_SH_USERNAME_COMPLETION

static char **username_tab_completion(char *ud, int *num_matches)
{
	struct passwd *entry;
	int userlen;
	char *temp;


	ud++;						/* ~user/... to user/... */
	userlen = strlen(ud);

	if (num_matches == 0) {		/* "~/..." or "~user/..." */
		char *sav_ud = ud - 1;
		char *home = 0;

		if (*ud == '/') {		/* "~/..."     */
			home = home_pwd_buf;
		} else {
			/* "~user/..." */
			temp = strchr(ud, '/');
			*temp = 0;			/* ~user\0 */
			entry = getpwnam(ud);
			*temp = '/';		/* restore ~user/... */
			ud = temp;
			if (entry)
				home = entry->pw_dir;
		}
		if (home) {
			if ((userlen + strlen(home) + 1) < BUFSIZ) {
				char temp2[BUFSIZ];	/* argument size */

				/* /home/user/... */
				sprintf(temp2, "%s%s", home, ud);
				strcpy(sav_ud, temp2);
			}
		}
		return 0;				/* void, result save to argument :-) */
	} else {
		/* "~[^/]*" */
		char **matches = (char **) NULL;
		int nm = 0;

		setpwent();

		while ((entry = getpwent()) != NULL) {
			/* Null usernames should result in all users as possible completions. */
			if ( /*!userlen || */ !strncmp(ud, entry->pw_name, userlen)) {

				temp = xmalloc(3 + strlen(entry->pw_name));
				sprintf(temp, "~%s/", entry->pw_name);
				matches = xrealloc(matches, (nm + 1) * sizeof(char *));

				matches[nm++] = temp;
			}
		}

		endpwent();
		(*num_matches) = nm;
		return (matches);
	}
}
#endif							/* BB_FEATURE_SH_USERNAME_COMPLETION */

enum {
	FIND_EXE_ONLY = 0,
	FIND_DIR_ONLY = 1,
	FIND_FILE_ONLY = 2,
};

static int path_parse(char ***p, int flags)
{
	int npth;
	char *tmp;
	char *pth;

	/* if not setenv PATH variable, to search cur dir "." */
	if (flags != FIND_EXE_ONLY || (pth = getenv("PATH")) == 0 ||
		/* PATH=<empty> or PATH=:<empty> */
		*pth == 0 || (*pth == ':' && *(pth + 1) == 0)) {
		return 1;
	}

	tmp = pth;
	npth = 0;

	for (;;) {
		npth++;					/* count words is + 1 count ':' */
		tmp = strchr(tmp, ':');
		if (tmp) {
			if (*++tmp == 0)
				break;			/* :<empty> */
		} else
			break;
	}

	*p = xmalloc(npth * sizeof(char *));

	tmp = pth;
	(*p)[0] = xstrdup(tmp);
	npth = 1;					/* count words is + 1 count ':' */

	for (;;) {
		tmp = strchr(tmp, ':');
		if (tmp) {
			(*p)[0][(tmp - pth)] = 0;	/* ':' -> '\0' */
			if (*++tmp == 0)
				break;			/* :<empty> */
		} else
			break;
		(*p)[npth++] = &(*p)[0][(tmp - pth)];	/* p[next]=p[0][&'\0'+1] */
	}

	return npth;
}

static char *add_quote_for_spec_chars(char *found)
{
	int l = 0;
	char *s = xmalloc((strlen(found) + 1) * 2);

	while (*found) {
		if (strchr(" `\"#$%^&*()=+{}[]:;\'|\\<>", *found))
			s[l++] = '\\';
		s[l++] = *found++;
	}
	s[l] = 0;
	return s;
}

static char **exe_n_cwd_tab_completion(char *command, int *num_matches,
									   int type)
{

	char **matches = 0;
	DIR *dir;
	struct dirent *next;
	char dirbuf[BUFSIZ];
	int nm = *num_matches;
	struct stat st;
	char *path1[1];
	char **paths = path1;
	int npaths;
	int i;
	char found[BUFSIZ + 4 + PATH_MAX];
	char *pfind = strrchr(command, '/');

	path1[0] = ".";

	if (pfind == NULL) {
		/* no dir, if flags==EXE_ONLY - get paths, else "." */
		npaths = path_parse(&paths, type);
		pfind = command;
	} else {
		/* with dir */
		/* save for change */
		strcpy(dirbuf, command);
		/* set dir only */
		dirbuf[(pfind - command) + 1] = 0;
#ifdef BB_FEATURE_SH_USERNAME_COMPLETION
		if (dirbuf[0] == '~')	/* ~/... or ~user/... */
			username_tab_completion(dirbuf, 0);
#endif
		/* "strip" dirname in command */
		pfind++;

		paths[0] = dirbuf;
		npaths = 1;				/* only 1 dir */
	}

	for (i = 0; i < npaths; i++) {

		dir = opendir(paths[i]);
		if (!dir)				/* Don't print an error */
			continue;

		while ((next = readdir(dir)) != NULL) {
			const char *str_merge = "%s/%s";
			char *str_found = next->d_name;

			/* matched ? */
			if (strncmp(str_found, pfind, strlen(pfind)))
				continue;
			/* not see .name without .match */
			if (*str_found == '.' && *pfind == 0) {
				if (*paths[i] == '/' && paths[i][1] == 0
					&& str_found[1] == 0) str_found = "";	/* only "/" */
				else
					continue;
			}
			if (paths[i][strlen(paths[i]) - 1] == '/')
				str_merge = "%s%s";
			sprintf(found, str_merge, paths[i], str_found);
			/* hmm, remover in progress? */
			if (stat(found, &st) < 0)
				continue;
			/* find with dirs ? */
			if (paths[i] != dirbuf)
				strcpy(found, next->d_name);	/* only name */
			if (S_ISDIR(st.st_mode)) {
				/* name is directory      */
				/* algorithmic only "/" ? */
				if (*str_found)
					strcat(found, "/");
				str_found = add_quote_for_spec_chars(found);
			} else {
				/* not put found file if search only dirs for cd */
				if (type == FIND_DIR_ONLY)
					continue;
				str_found = add_quote_for_spec_chars(found);
				if (type == FIND_FILE_ONLY ||
					(type == FIND_EXE_ONLY && is_execute(&st) == TRUE))
					strcat(str_found, " ");
			}
			/* Add it to the list */
			matches = xrealloc(matches, (nm + 1) * sizeof(char *));

			matches[nm++] = str_found;
		}
		closedir(dir);
	}
	if (paths != path1) {
		free(paths[0]);			/* allocated memory only in first member */
		free(paths);
	}
	*num_matches = nm;
	return (matches);
}

static int match_compare(const void *a, const void *b)
{
	return strcmp(*(char **) a, *(char **) b);
}



#define QUOT    (UCHAR_MAX+1)

#define collapse_pos(is, in) { \
	memcpy(int_buf+is, int_buf+in, (BUFSIZ+1-is-in)*sizeof(int)); \
	memcpy(pos_buf+is, pos_buf+in, (BUFSIZ+1-is-in)*sizeof(int)); }

static int find_match(char *matchBuf, int *len_with_quotes)
{
	int i, j;
	int command_mode;
	int c, c2;
	int int_buf[BUFSIZ + 1];
	int pos_buf[BUFSIZ + 1];

	/* set to integer dimension characters and own positions */
	for (i = 0;; i++) {
		int_buf[i] = (int) ((unsigned char) matchBuf[i]);
		if (int_buf[i] == 0) {
			pos_buf[i] = -1;	/* indicator end line */
			break;
		} else
			pos_buf[i] = i;
	}

	/* mask \+symbol and convert '\t' to ' ' */
	for (i = j = 0; matchBuf[i]; i++, j++)
		if (matchBuf[i] == '\\') {
			collapse_pos(j, j + 1);
			int_buf[j] |= QUOT;
			i++;
#ifdef BB_FEATURE_NONPRINTABLE_INVERSE_PUT
			if (matchBuf[i] == '\t')	/* algorithm equivalent */
				int_buf[j] = ' ' | QUOT;
#endif
		}
#ifdef BB_FEATURE_NONPRINTABLE_INVERSE_PUT
		else if (matchBuf[i] == '\t')
			int_buf[j] = ' ';
#endif

	/* mask "symbols" or 'symbols' */
	c2 = 0;
	for (i = 0; int_buf[i]; i++) {
		c = int_buf[i];
		if (c == '\'' || c == '"') {
			if (c2 == 0)
				c2 = c;
			else {
				if (c == c2)
					c2 = 0;
				else
					int_buf[i] |= QUOT;
			}
		} else if (c2 != 0 && c != '$')
			int_buf[i] |= QUOT;
	}

	/* skip commands with arguments if line have commands delimiters */
	/* ';' ';;' '&' '|' '&&' '||' but `>&' `<&' `>|' */
	for (i = 0; int_buf[i]; i++) {
		c = int_buf[i];
		c2 = int_buf[i + 1];
		j = i ? int_buf[i - 1] : -1;
		command_mode = 0;
		if (c == ';' || c == '&' || c == '|') {
			command_mode = 1 + (c == c2);
			if (c == '&') {
				if (j == '>' || j == '<')
					command_mode = 0;
			} else if (c == '|' && j == '>')
				command_mode = 0;
		}
		if (command_mode) {
			collapse_pos(0, i + command_mode);
			i = -1;				/* hack incremet */
		}
	}
	/* collapse `command...` */
	for (i = 0; int_buf[i]; i++)
		if (int_buf[i] == '`') {
			for (j = i + 1; int_buf[j]; j++)
				if (int_buf[j] == '`') {
					collapse_pos(i, j + 1);
					j = 0;
					break;
				}
			if (j) {
				/* not found close ` - command mode, collapse all previous */
				collapse_pos(0, i + 1);
				break;
			} else
				i--;			/* hack incremet */
		}

	/* collapse (command...(command...)...) or {command...{command...}...} */
	c = 0;						/* "recursive" level */
	c2 = 0;
	for (i = 0; int_buf[i]; i++)
		if (int_buf[i] == '(' || int_buf[i] == '{') {
			if (int_buf[i] == '(')
				c++;
			else
				c2++;
			collapse_pos(0, i + 1);
			i = -1;				/* hack incremet */
		}
	for (i = 0; pos_buf[i] >= 0 && (c > 0 || c2 > 0); i++)
		if ((int_buf[i] == ')' && c > 0) || (int_buf[i] == '}' && c2 > 0)) {
			if (int_buf[i] == ')')
				c--;
			else
				c2--;
			collapse_pos(0, i + 1);
			i = -1;				/* hack incremet */
		}

	/* skip first not quote space */
	for (i = 0; int_buf[i]; i++)
		if (int_buf[i] != ' ')
			break;
	if (i)
		collapse_pos(0, i);

	/* set find mode for completion */
	command_mode = FIND_EXE_ONLY;
	for (i = 0; int_buf[i]; i++)
		if (int_buf[i] == ' ' || int_buf[i] == '<' || int_buf[i] == '>') {
			if (int_buf[i] == ' ' && command_mode == FIND_EXE_ONLY
				&& strncmp(&matchBuf[pos_buf[0]], "cd", 2) == 0)
				command_mode = FIND_DIR_ONLY;
			else {
				command_mode = FIND_FILE_ONLY;
				break;
			}
		}
	/* "strlen" */
	for (i = 0; int_buf[i]; i++);
	/* find last word */
	for (--i; i >= 0; i--) {
		c = int_buf[i];
		if (c == ' ' || c == '<' || c == '>' || c == '|' || c == '&') {
			collapse_pos(0, i + 1);
			break;
		}
	}
	/* skip first not quoted '\'' or '"' */
	for (i = 0; int_buf[i] == '\'' || int_buf[i] == '"'; i++);
	/* collapse quote or unquote // or /~ */
	while ((int_buf[i] & ~QUOT) == '/' && (
										   (int_buf[i + 1] & ~QUOT) == '/'
										   || (int_buf[i + 1] & ~QUOT) ==
										   '~')) i++;
	if (i)
		collapse_pos(0, i);

	/* set only match and destroy quotes */
	j = 0;
	for (i = 0; pos_buf[i] >= 0; i++) {
		matchBuf[i] = matchBuf[pos_buf[i]];
		j = pos_buf[i] + 1;
	}
	matchBuf[i] = 0;
	/* old lenght matchBuf with quotes symbols */
	*len_with_quotes = j ? j - pos_buf[0] : 0;

	return command_mode;
}


static void input_tab(int *lastWasTab)
{
	/* Do TAB completion */
	static int num_matches;
	static char **matches;

	if (lastWasTab == 0) {		/* free all memory */
		if (matches) {
			while (num_matches > 0)
				free(matches[--num_matches]);
			free(matches);
			matches = (char **) NULL;
		}
		return;
	}
	if (*lastWasTab == FALSE) {

		char *tmp;
		int len_found;
		char matchBuf[BUFSIZ];
		int find_type;
		int recalc_pos;

		*lastWasTab = TRUE;		/* flop trigger */

		/* Make a local copy of the string -- up
		 * to the position of the cursor */
		tmp = strncpy(matchBuf, command_ps, cursor);
		tmp[cursor] = 0;

		find_type = find_match(matchBuf, &recalc_pos);

		/* Free up any memory already allocated */
		input_tab(0);

#ifdef BB_FEATURE_SH_USERNAME_COMPLETION
		/* If the word starts with `~' and there is no slash in the word,
		 * then try completing this word as a username. */

		if (matchBuf[0] == '~' && strchr(matchBuf, '/') == 0)
			matches = username_tab_completion(matchBuf, &num_matches);
#endif
		/* Try to match any executable in our path and everything
		 * in the current working directory that matches.  */
		if (!matches)
			matches =
				exe_n_cwd_tab_completion(matchBuf, &num_matches,
										 find_type);

		/* Did we find exactly one match? */
		if (!matches || num_matches > 1) {
			char *tmp1;

			beep();
			if (!matches)
				return;			/* not found */
			/* sort */
			qsort(matches, num_matches, sizeof(char *), match_compare);

			/* find minimal match */
			tmp = xstrdup(matches[0]);
			for (tmp1 = tmp; *tmp1; tmp1++)
				for (len_found = 1; len_found < num_matches; len_found++)
					if (matches[len_found][(tmp1 - tmp)] != *tmp1) {
						*tmp1 = 0;
						break;
					}
			if (*tmp == 0) {	/* have unique */
				free(tmp);
				return;
			}
		} else {				/* one match */
			tmp = matches[0];
			/* for next completion current found */
			*lastWasTab = FALSE;
		}

		len_found = strlen(tmp);
		/* have space to placed match? */
		if ((len_found - strlen(matchBuf) + len) < BUFSIZ) {

			/* before word for match   */
			command_ps[cursor - recalc_pos] = 0;
			/* save   tail line        */
			strcpy(matchBuf, command_ps + cursor);
			/* add    match            */
			strcat(command_ps, tmp);
			/* add    tail             */
			strcat(command_ps, matchBuf);
			/* back to begin word for match    */
			input_backward(recalc_pos);
			/* new pos                         */
			recalc_pos = cursor + len_found;
			/* new len                         */
			len = strlen(command_ps);
			/* write out the matched command   */
			input_end();
			input_backward(cursor - recalc_pos);
		}
		if (tmp != matches[0])
			free(tmp);
	} else {
		/* Ok -- the last char was a TAB.  Since they
		 * just hit TAB again, print a list of all the
		 * available choices... */
		if (matches && num_matches > 0) {
			int i, col, l;
			int sav_cursor = cursor;	/* change goto_new_line() */

			/* Go to the next line */
			goto_new_line();
			for (i = 0, col = 0; i < num_matches; i++) {
				l = strlen(matches[i]);
				if (l < 14)
					l = 14;
				printf("%-14s  ", matches[i]);
				if ((l += 2) > 16)
					while (l % 16) {
						putchar(' ');
						l++;
					}
				col += l;
				col -= (col / cmdedit_termw) * cmdedit_termw;
				if (col > 60 && matches[i + 1] != NULL) {
					putchar('\n');
					col = 0;
				}
			}
			/* Go to the next line and rewrite */
			putchar('\n');
			redraw(0, len - sav_cursor);
		}
	}
}
#endif							/* BB_FEATURE_SH_TAB_COMPLETION */

static void get_previous_history(struct history **hp, struct history *p)
{
	if ((*hp)->s)
		free((*hp)->s);
	(*hp)->s = xstrdup(command_ps);
	*hp = p;
}

static inline void get_next_history(struct history **hp)
{
	get_previous_history(hp, (*hp)->n);
}

enum {
	ESC = 27,
	DEL = 127,
};


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
 */
extern void cmdedit_read_input(char *prompt, char command[BUFSIZ])
{

	int inputFd = fileno(stdin);

	int break_out = 0;
	int lastWasTab = FALSE;
	char c = 0;
	struct history *hp = his_end;

	/* prepare before init handlers */
	cmdedit_y = 0;				/* quasireal y, not true work if line > xt*yt */
	len = 0;
	command_ps = command;

	if (new_settings.c_cc[VMIN] == 0) {	/* first call */

		getTermSettings(inputFd, (void *) &initial_settings);
		memcpy(&new_settings, &initial_settings, sizeof(struct termios));

		new_settings.c_cc[VMIN] = 1;
		new_settings.c_cc[VTIME] = 0;
		new_settings.c_cc[VINTR] = _POSIX_VDISABLE;	/* Turn off CTRL-C, so we can trap it */
		new_settings.c_lflag &= ~ICANON;	/* unbuffered input */
		new_settings.c_lflag &= ~(ECHO | ECHOCTL | ECHONL);	/* Turn off echoing */
	}

	command[0] = 0;

	setTermSettings(inputFd, (void *) &new_settings);
	handlers_sets |= SET_RESET_TERM;

	/* Print out the command prompt */
	parse_prompt(prompt);
	/* Now initialize things */
	cmdedit_init();

	while (1) {

		fflush(stdout);			/* buffered out to fast */

		if (read(inputFd, &c, 1) < 1)
			return;

		switch (c) {
		case '\n':
		case '\r':
			/* Enter */
			goto_new_line();
			break_out = 1;
			break;
		case 1:
			/* Control-a -- Beginning of line */
			input_backward(cursor);
			break;
		case 2:
			/* Control-b -- Move back one character */
			input_backward(1);
			break;
		case 3:
			/* Control-c -- stop gathering input */

			/* Link into lash to reset context to 0 on ^C and such */
			shell_context = 0;

			/* Go to the next line */
			goto_new_line();
			command[0] = 0;

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
			input_tab(&lastWasTab);
#endif
			break;
		case 14:
			/* Control-n -- Get next command in history */
			if (hp && hp->n && hp->n->s) {
				get_next_history(&hp);
				goto rewrite_line;
			} else {
				beep();
			}
			break;
		case 16:
			/* Control-p -- Get previous command from history */
			if (hp && hp->p) {
				get_previous_history(&hp, hp->p);
				goto rewrite_line;
			} else {
				beep();
			}
			break;
		case 21:
			/* Control-U -- Clear line before cursor */
			if (cursor) {
				strcpy(command, command + cursor);
				redraw(cmdedit_y, len -= cursor);
			}
			break;

		case ESC:{
			/* escape sequence follows */
			if (read(inputFd, &c, 1) < 1)
				return;
			/* different vt100 emulations */
			if (c == '[' || c == 'O') {
				if (read(inputFd, &c, 1) < 1)
					return;
			}
			switch (c) {
#ifdef BB_FEATURE_SH_TAB_COMPLETION
			case '\t':			/* Alt-Tab */

				input_tab(&lastWasTab);
				break;
#endif
			case 'A':
				/* Up Arrow -- Get previous command from history */
				if (hp && hp->p) {
					get_previous_history(&hp, hp->p);
					goto rewrite_line;
				} else {
					beep();
				}
				break;
			case 'B':
				/* Down Arrow -- Get next command in history */
				if (hp && hp->n && hp->n->s) {
					get_next_history(&hp);
					goto rewrite_line;
				} else {
					beep();
				}
				break;

				/* Rewrite the line with the selected history item */
			  rewrite_line:
				/* change command */
				len = strlen(strcpy(command, hp->s));
				/* redraw and go to end line */
				redraw(cmdedit_y, 0);
				break;
			case 'C':
				/* Right Arrow -- Move forward one character */
				input_forward();
				break;
			case 'D':
				/* Left Arrow -- Move back one character */
				input_backward(1);
				break;
			case '3':
				/* Delete */
				input_delete();
				break;
			case '1':
			case 'H':
				/* Home (Ctrl-A) */
				input_backward(cursor);
				break;
			case '4':
			case 'F':
				/* End (Ctrl-E) */
				input_end();
				break;
			default:
				if (!(c >= '1' && c <= '9'))
					c = 0;
				beep();
			}
			if (c >= '1' && c <= '9')
				do
					if (read(inputFd, &c, 1) < 1)
						return;
				while (c != '~');
			break;
		}

		default:				/* If it's regular input, do the normal thing */
#ifdef BB_FEATURE_NONPRINTABLE_INVERSE_PUT
			/* Control-V -- Add non-printable symbol */
			if (c == 22) {
				if (read(inputFd, &c, 1) < 1)
					return;
				if (c == 0) {
					beep();
					break;
				}
			} else
#endif
			if (!isprint(c))	/* Skip non-printable characters */
				break;

			if (len >= (BUFSIZ - 2))	/* Need to leave space for enter */
				break;

			len++;

			if (cursor == (len - 1)) {	/* Append if at the end of the line */
				*(command + cursor) = c;
				*(command + cursor + 1) = 0;
				cmdedit_set_out_char(0);
			} else {			/* Insert otherwise */
				int sc = cursor;

				memmove(command + sc + 1, command + sc, len - sc);
				*(command + sc) = c;
				sc++;
				/* rewrite from cursor */
				input_end();
				/* to prev x pos + 1 */
				input_backward(cursor - sc);
			}

			break;
		}
		if (break_out)			/* Enter is the command terminator, no more input. */
			break;

		if (c != '\t')
			lastWasTab = FALSE;
	}

	setTermSettings(inputFd, (void *) &initial_settings);
	handlers_sets &= ~SET_RESET_TERM;

	/* Handle command history log */
	if (len) {					/* no put empty line */

		struct history *h = his_end;
		char *ss;

		ss = xstrdup(command);	/* duplicate */

		if (h == 0) {
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
#if defined(BB_FEATURE_BASH_STYLE_PROMT)
		num_ok_lines++;
#endif
	}
	command[len++] = '\n';		/* set '\n' */
	command[len] = 0;
#if defined(BB_FEATURE_CLEAN_UP) && defined(BB_FEATURE_SH_TAB_COMPLETION)
	input_tab(0);				/* strong free */
#endif
#if defined(BB_FEATURE_BASH_STYLE_PROMT)
	free(cmdedit_prompt);
#endif
	return;
}


/* Undo the effects of cmdedit_init(). */
extern void cmdedit_terminate(void)
{
	cmdedit_reset_term();
	if ((handlers_sets & SET_TERM_HANDLERS) != 0) {
		signal(SIGKILL, SIG_DFL);
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		signal(SIGTERM, SIG_DFL);
		signal(SIGWINCH, SIG_DFL);
		handlers_sets &= ~SET_TERM_HANDLERS;
	}
}

#endif							/* BB_FEATURE_SH_COMMAND_EDITING */


#ifdef TEST

unsigned int shell_context;

int main(int argc, char **argv)
{
	char buff[BUFSIZ];
	char *prompt =
#if defined(BB_FEATURE_BASH_STYLE_PROMT)
		"\\[\\033[32;1m\\]\\u@\\[\\033[33;1m\\]\\h:\
\\[\\033[34;1m\\]\\w\\[\\033[35;1m\\] \
\\!\\[\\033[36;1m\\]\\$ \\[\\033[0m\\]";
#else
		"% ";
#endif

	shell_context = 1;
	do {
		cmdedit_read_input(prompt, buff);
		printf("*** cmdedit_read_input() returned line =%s=\n", buff);
	} while (shell_context);
	printf("*** cmdedit_read_input() detect ^C\n");
	return 0;
}

#endif							/* TEST */
