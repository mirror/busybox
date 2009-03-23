/* vi: set sw=4 ts=4: */
/*
 * Termios command line History and Editing.
 *
 * Copyright (c) 1986-2003 may safely be consumed by a BSD or GPL license.
 * Written by:   Vladimir Oleynik <dzo@simtreas.ru>
 *
 * Used ideas:
 *      Adam Rogoyski    <rogoyski@cs.utexas.edu>
 *      Dave Cinege      <dcinege@psychosis.com>
 *      Jakub Jelinek (c) 1995
 *      Erik Andersen    <andersen@codepoet.org> (Majorly adjusted for busybox)
 *
 * This code is 'as is' with no warranty.
 */

/*
 * Usage and known bugs:
 * Terminal key codes are not extensive, and more will probably
 * need to be added. This version was created on Debian GNU/Linux 2.x.
 * Delete, Backspace, Home, End, and the arrow keys were tested
 * to work in an Xterm and console. Ctrl-A also works as Home.
 * Ctrl-E also works as End.
 *
 * lineedit does not know that the terminal escape sequences do not
 * take up space on the screen. The redisplay code assumes, unless
 * told otherwise, that each character in the prompt is a printable
 * character that takes up one character position on the screen.
 * You need to tell lineedit that some sequences of characters
 * in the prompt take up no screen space. Compatibly with readline,
 * use the \[ escape to begin a sequence of non-printing characters,
 * and the \] escape to signal the end of such a sequence. Example:
 *
 * PS1='\[\033[01;32m\]\u@\h\[\033[01;34m\] \w \$\[\033[00m\] '
 */

#include "libbb.h"


/* FIXME: obsolete CONFIG item? */
#define ENABLE_FEATURE_NONPRINTABLE_INVERSE_PUT 0


#ifdef TEST

#define ENABLE_FEATURE_EDITING 0
#define ENABLE_FEATURE_TAB_COMPLETION 0
#define ENABLE_FEATURE_USERNAME_COMPLETION 0
#define ENABLE_FEATURE_NONPRINTABLE_INVERSE_PUT 0

#endif  /* TEST */


/* Entire file (except TESTing part) sits inside this #if */
#if ENABLE_FEATURE_EDITING

#if ENABLE_LOCALE_SUPPORT
#define Isprint(c) isprint(c)
#else
#define Isprint(c) ((c) >= ' ' && (c) != ((unsigned char)'\233'))
#endif

#define ENABLE_FEATURE_GETUSERNAME_AND_HOMEDIR \
	(ENABLE_FEATURE_USERNAME_COMPLETION || ENABLE_FEATURE_EDITING_FANCY_PROMPT)
#define USE_FEATURE_GETUSERNAME_AND_HOMEDIR(...)
#if ENABLE_FEATURE_GETUSERNAME_AND_HOMEDIR
#undef USE_FEATURE_GETUSERNAME_AND_HOMEDIR
#define USE_FEATURE_GETUSERNAME_AND_HOMEDIR(...) __VA_ARGS__
#endif

enum {
	/* We use int16_t for positions, need to limit line len */
	MAX_LINELEN = CONFIG_FEATURE_EDITING_MAX_LEN < 0x7ff0
	              ? CONFIG_FEATURE_EDITING_MAX_LEN
	              : 0x7ff0
};

#if ENABLE_FEATURE_GETUSERNAME_AND_HOMEDIR
static const char null_str[] ALIGN1 = "";
#endif

/* We try to minimize both static and stack usage. */
struct lineedit_statics {
	line_input_t *state;

	volatile unsigned cmdedit_termw; /* = 80; */ /* actual terminal width */
	sighandler_t previous_SIGWINCH_handler;

	unsigned cmdedit_x;        /* real x terminal position */
	unsigned cmdedit_y;        /* pseudoreal y terminal position */
	unsigned cmdedit_prmt_len; /* length of prompt (without colors etc) */

	unsigned cursor;
	unsigned command_len;
	char *command_ps;

	const char *cmdedit_prompt;
#if ENABLE_FEATURE_EDITING_FANCY_PROMPT
	int num_ok_lines; /* = 1; */
#endif

#if ENABLE_FEATURE_GETUSERNAME_AND_HOMEDIR
	char *user_buf;
	char *home_pwd_buf; /* = (char*)null_str; */
#endif

#if ENABLE_FEATURE_TAB_COMPLETION
	char **matches;
	unsigned num_matches;
#endif

#if ENABLE_FEATURE_EDITING_VI
#define DELBUFSIZ 128
	char *delptr;
	smallint newdelflag;     /* whether delbuf should be reused yet */
	char delbuf[DELBUFSIZ];  /* a place to store deleted characters */
#endif

	/* Formerly these were big buffers on stack: */
#if ENABLE_FEATURE_TAB_COMPLETION
	char exe_n_cwd_tab_completion__dirbuf[MAX_LINELEN];
	char input_tab__matchBuf[MAX_LINELEN];
	int16_t find_match__int_buf[MAX_LINELEN + 1]; /* need to have 9 bits at least */
	int16_t find_match__pos_buf[MAX_LINELEN + 1];
#endif
};

/* See lineedit_ptr_hack.c */
extern struct lineedit_statics *const lineedit_ptr_to_statics;

#define S (*lineedit_ptr_to_statics)
#define state            (S.state           )
#define cmdedit_termw    (S.cmdedit_termw   )
#define previous_SIGWINCH_handler (S.previous_SIGWINCH_handler)
#define cmdedit_x        (S.cmdedit_x       )
#define cmdedit_y        (S.cmdedit_y       )
#define cmdedit_prmt_len (S.cmdedit_prmt_len)
#define cursor           (S.cursor          )
#define command_len      (S.command_len     )
#define command_ps       (S.command_ps      )
#define cmdedit_prompt   (S.cmdedit_prompt  )
#define num_ok_lines     (S.num_ok_lines    )
#define user_buf         (S.user_buf        )
#define home_pwd_buf     (S.home_pwd_buf    )
#define matches          (S.matches         )
#define num_matches      (S.num_matches     )
#define delptr           (S.delptr          )
#define newdelflag       (S.newdelflag      )
#define delbuf           (S.delbuf          )

#define INIT_S() do { \
	(*(struct lineedit_statics**)&lineedit_ptr_to_statics) = xzalloc(sizeof(S)); \
	barrier(); \
	cmdedit_termw = 80; \
	USE_FEATURE_EDITING_FANCY_PROMPT(num_ok_lines = 1;) \
	USE_FEATURE_GETUSERNAME_AND_HOMEDIR(home_pwd_buf = (char*)null_str;) \
} while (0)
static void deinit_S(void)
{
#if ENABLE_FEATURE_EDITING_FANCY_PROMPT
	/* This one is allocated only if FANCY_PROMPT is on
	 * (otherwise it points to verbatim prompt (NOT malloced) */
	free((char*)cmdedit_prompt);
#endif
#if ENABLE_FEATURE_GETUSERNAME_AND_HOMEDIR
	free(user_buf);
	if (home_pwd_buf != null_str)
		free(home_pwd_buf);
#endif
	free(lineedit_ptr_to_statics);
}
#define DEINIT_S() deinit_S()


/* Put 'command_ps[cursor]', cursor++.
 * Advance cursor on screen. If we reached right margin, scroll text up
 * and remove terminal margin effect by printing 'next_char' */
#define HACK_FOR_WRONG_WIDTH 1
#if HACK_FOR_WRONG_WIDTH
static void cmdedit_set_out_char(void)
#define cmdedit_set_out_char(next_char) cmdedit_set_out_char()
#else
static void cmdedit_set_out_char(int next_char)
#endif
{
	int c = (unsigned char)command_ps[cursor];

	if (c == '\0') {
		/* erase character after end of input string */
		c = ' ';
	}
#if ENABLE_FEATURE_NONPRINTABLE_INVERSE_PUT
	/* Display non-printable characters in reverse */
	if (!Isprint(c)) {
		if (c >= 128)
			c -= 128;
		if (c < ' ')
			c += '@';
		if (c == 127)
			c = '?';
		printf("\033[7m%c\033[0m", c);
	} else
#endif
	{
		bb_putchar(c);
	}
	if (++cmdedit_x >= cmdedit_termw) {
		/* terminal is scrolled down */
		cmdedit_y++;
		cmdedit_x = 0;
#if HACK_FOR_WRONG_WIDTH
		/* This works better if our idea of term width is wrong
		 * and it is actually wider (often happens on serial lines).
		 * Printing CR,LF *forces* cursor to next line.
		 * OTOH if terminal width is correct AND terminal does NOT
		 * have automargin (IOW: it is moving cursor to next line
		 * by itself (which is wrong for VT-10x terminals)),
		 * this will break things: there will be one extra empty line */
		puts("\r"); /* + implicit '\n' */
#else
		/* Works ok only if cmdedit_termw is correct */
		/* destroy "(auto)margin" */
		bb_putchar(next_char);
		bb_putchar('\b');
#endif
	}
// Huh? What if command_ps[cursor] == '\0' (we are at the end already?)
	cursor++;
}

/* Move to end of line (by printing all chars till the end) */
static void input_end(void)
{
	while (cursor < command_len)
		cmdedit_set_out_char(' ');
}

/* Go to the next line */
static void goto_new_line(void)
{
	input_end();
	if (cmdedit_x)
		bb_putchar('\n');
}


static void out1str(const char *s)
{
	if (s)
		fputs(s, stdout);
}

static void beep(void)
{
	bb_putchar('\007');
}

/* Move back one character */
/* (optimized for slow terminals) */
static void input_backward(unsigned num)
{
	int count_y;

	if (num > cursor)
		num = cursor;
	if (!num)
		return;
	cursor -= num;

	if (cmdedit_x >= num) {
		cmdedit_x -= num;
		if (num <= 4) {
			/* This is longer by 5 bytes on x86.
			 * Also gets miscompiled for ARM users
			 * (busybox.net/bugs/view.php?id=2274).
			 * printf(("\b\b\b\b" + 4) - num);
			 * return;
			 */
			do {
				bb_putchar('\b');
			} while (--num);
			return;
		}
		printf("\033[%uD", num);
		return;
	}

	/* Need to go one or more lines up */
	num -= cmdedit_x;
	{
		unsigned w = cmdedit_termw; /* volatile var */
		count_y = 1 + (num / w);
		cmdedit_y -= count_y;
		cmdedit_x = w * count_y - num;
	}
	/* go to 1st column; go up; go to correct column */
	printf("\r" "\033[%dA" "\033[%dC", count_y, cmdedit_x);
}

static void put_prompt(void)
{
	out1str(cmdedit_prompt);
	cursor = 0;
	{
		unsigned w = cmdedit_termw; /* volatile var */
		cmdedit_y = cmdedit_prmt_len / w; /* new quasireal y */
		cmdedit_x = cmdedit_prmt_len % w;
	}
}

/* draw prompt, editor line, and clear tail */
static void redraw(int y, int back_cursor)
{
	if (y > 0)                              /* up to start y */
		printf("\033[%dA", y);
	bb_putchar('\r');
	put_prompt();
	input_end();                            /* rewrite */
	printf("\033[J");                       /* erase after cursor */
	input_backward(back_cursor);
}

/* Delete the char in front of the cursor, optionally saving it
 * for later putback */
#if !ENABLE_FEATURE_EDITING_VI
static void input_delete(void)
#define input_delete(save) input_delete()
#else
static void input_delete(int save)
#endif
{
	int j = cursor;

	if (j == (int)command_len)
		return;

#if ENABLE_FEATURE_EDITING_VI
	if (save) {
		if (newdelflag) {
			delptr = delbuf;
			newdelflag = 0;
		}
		if ((delptr - delbuf) < DELBUFSIZ)
			*delptr++ = command_ps[j];
	}
#endif

	overlapping_strcpy(command_ps + j, command_ps + j + 1);
	command_len--;
	input_end();                    /* rewrite new line */
	cmdedit_set_out_char(' ');      /* erase char */
	input_backward(cursor - j);     /* back to old pos cursor */
}

#if ENABLE_FEATURE_EDITING_VI
static void put(void)
{
	int ocursor;
	int j = delptr - delbuf;

	if (j == 0)
		return;
	ocursor = cursor;
	/* open hole and then fill it */
	memmove(command_ps + cursor + j, command_ps + cursor, command_len - cursor + 1);
	strncpy(command_ps + cursor, delbuf, j);
	command_len += j;
	input_end();                    /* rewrite new line */
	input_backward(cursor - ocursor - j + 1); /* at end of new text */
}
#endif

/* Delete the char in back of the cursor */
static void input_backspace(void)
{
	if (cursor > 0) {
		input_backward(1);
		input_delete(0);
	}
}

/* Move forward one character */
static void input_forward(void)
{
	if (cursor < command_len)
		cmdedit_set_out_char(command_ps[cursor + 1]);
}

#if ENABLE_FEATURE_TAB_COMPLETION

static void free_tab_completion_data(void)
{
	if (matches) {
		while (num_matches)
			free(matches[--num_matches]);
		free(matches);
		matches = NULL;
	}
}

static void add_match(char *matched)
{
	matches = xrealloc_vector(matches, 4, num_matches);
	matches[num_matches] = matched;
	num_matches++;
}

#if ENABLE_FEATURE_USERNAME_COMPLETION
static void username_tab_completion(char *ud, char *with_shash_flg)
{
	struct passwd *entry;
	int userlen;

	ud++;                           /* ~user/... to user/... */
	userlen = strlen(ud);

	if (with_shash_flg) {           /* "~/..." or "~user/..." */
		char *sav_ud = ud - 1;
		char *home = NULL;

		if (*ud == '/') {       /* "~/..."     */
			home = home_pwd_buf;
		} else {
			/* "~user/..." */
			char *temp;
			temp = strchr(ud, '/');
			*temp = '\0';           /* ~user\0 */
			entry = getpwnam(ud);
			*temp = '/';            /* restore ~user/... */
			ud = temp;
			if (entry)
				home = entry->pw_dir;
		}
		if (home) {
			if ((userlen + strlen(home) + 1) < MAX_LINELEN) {
				/* /home/user/... */
				sprintf(sav_ud, "%s%s", home, ud);
			}
		}
	} else {
		/* "~[^/]*" */
		/* Using _r function to avoid pulling in static buffers */
		char line_buff[256];
		struct passwd pwd;
		struct passwd *result;

		setpwent();
		while (!getpwent_r(&pwd, line_buff, sizeof(line_buff), &result)) {
			/* Null usernames should result in all users as possible completions. */
			if (/*!userlen || */ strncmp(ud, pwd.pw_name, userlen) == 0) {
				add_match(xasprintf("~%s/", pwd.pw_name));
			}
		}
		endpwent();
	}
}
#endif  /* FEATURE_COMMAND_USERNAME_COMPLETION */

enum {
	FIND_EXE_ONLY = 0,
	FIND_DIR_ONLY = 1,
	FIND_FILE_ONLY = 2,
};

static int path_parse(char ***p, int flags)
{
	int npth;
	const char *pth;
	char *tmp;
	char **res;

	/* if not setenv PATH variable, to search cur dir "." */
	if (flags != FIND_EXE_ONLY)
		return 1;

	if (state->flags & WITH_PATH_LOOKUP)
		pth = state->path_lookup;
	else
		pth = getenv("PATH");
	/* PATH=<empty> or PATH=:<empty> */
	if (!pth || !pth[0] || LONE_CHAR(pth, ':'))
		return 1;

	tmp = (char*)pth;
	npth = 1; /* path component count */
	while (1) {
		tmp = strchr(tmp, ':');
		if (!tmp)
			break;
		if (*++tmp == '\0')
			break;  /* :<empty> */
		npth++;
	}

	res = xmalloc(npth * sizeof(char*));
	res[0] = tmp = xstrdup(pth);
	npth = 1;
	while (1) {
		tmp = strchr(tmp, ':');
		if (!tmp)
			break;
		*tmp++ = '\0'; /* ':' -> '\0' */
		if (*tmp == '\0')
			break; /* :<empty> */
		res[npth++] = tmp;
	}
	*p = res;
	return npth;
}

static void exe_n_cwd_tab_completion(char *command, int type)
{
	DIR *dir;
	struct dirent *next;
	struct stat st;
	char *path1[1];
	char **paths = path1;
	int npaths;
	int i;
	char *found;
	char *pfind = strrchr(command, '/');
/*	char dirbuf[MAX_LINELEN]; */
#define dirbuf (S.exe_n_cwd_tab_completion__dirbuf)

	npaths = 1;
	path1[0] = (char*)".";

	if (pfind == NULL) {
		/* no dir, if flags==EXE_ONLY - get paths, else "." */
		npaths = path_parse(&paths, type);
		pfind = command;
	} else {
		/* dirbuf = ".../.../.../" */
		safe_strncpy(dirbuf, command, (pfind - command) + 2);
#if ENABLE_FEATURE_USERNAME_COMPLETION
		if (dirbuf[0] == '~')   /* ~/... or ~user/... */
			username_tab_completion(dirbuf, dirbuf);
#endif
		paths[0] = dirbuf;
		/* point to 'l' in "..../last_component" */
		pfind++;
	}

	for (i = 0; i < npaths; i++) {
		dir = opendir(paths[i]);
		if (!dir)
			continue; /* don't print an error */

		while ((next = readdir(dir)) != NULL) {
			int len1;
			const char *str_found = next->d_name;

			/* matched? */
			if (strncmp(str_found, pfind, strlen(pfind)))
				continue;
			/* not see .name without .match */
			if (*str_found == '.' && *pfind == '\0') {
				if (NOT_LONE_CHAR(paths[i], '/') || str_found[1])
					continue;
				str_found = ""; /* only "/" */
			}
			found = concat_path_file(paths[i], str_found);
			/* hmm, remove in progress? */
			/* NB: stat() first so that we see is it a directory;
			 * but if that fails, use lstat() so that
			 * we still match dangling links */
			if (stat(found, &st) && lstat(found, &st))
				goto cont;
			/* find with dirs? */
			if (paths[i] != dirbuf)
				strcpy(found, next->d_name); /* only name */

			len1 = strlen(found);
			found = xrealloc(found, len1 + 2);
			found[len1] = '\0';
			found[len1+1] = '\0';

			if (S_ISDIR(st.st_mode)) {
				/* name is a directory */
				if (found[len1-1] != '/') {
					found[len1] = '/';
				}
			} else {
				/* not put found file if search only dirs for cd */
				if (type == FIND_DIR_ONLY)
					goto cont;
			}
			/* Add it to the list */
			add_match(found);
			continue;
 cont:
			free(found);
		}
		closedir(dir);
	}
	if (paths != path1) {
		free(paths[0]); /* allocated memory is only in first member */
		free(paths);
	}
#undef dirbuf
}

#define QUOT (UCHAR_MAX+1)

#define collapse_pos(is, in) do { \
	memmove(int_buf+(is), int_buf+(in), (MAX_LINELEN+1-(is)-(in)) * sizeof(pos_buf[0])); \
	memmove(pos_buf+(is), pos_buf+(in), (MAX_LINELEN+1-(is)-(in)) * sizeof(pos_buf[0])); \
} while (0)

static int find_match(char *matchBuf, int *len_with_quotes)
{
	int i, j;
	int command_mode;
	int c, c2;
/*	int16_t int_buf[MAX_LINELEN + 1]; */
/*	int16_t pos_buf[MAX_LINELEN + 1]; */
#define int_buf (S.find_match__int_buf)
#define pos_buf (S.find_match__pos_buf)

	/* set to integer dimension characters and own positions */
	for (i = 0;; i++) {
		int_buf[i] = (unsigned char)matchBuf[i];
		if (int_buf[i] == 0) {
			pos_buf[i] = -1;        /* indicator end line */
			break;
		}
		pos_buf[i] = i;
	}

	/* mask \+symbol and convert '\t' to ' ' */
	for (i = j = 0; matchBuf[i]; i++, j++)
		if (matchBuf[i] == '\\') {
			collapse_pos(j, j + 1);
			int_buf[j] |= QUOT;
			i++;
#if ENABLE_FEATURE_NONPRINTABLE_INVERSE_PUT
			if (matchBuf[i] == '\t')        /* algorithm equivalent */
				int_buf[j] = ' ' | QUOT;
#endif
		}
#if ENABLE_FEATURE_NONPRINTABLE_INVERSE_PUT
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

	/* skip commands with arguments if line has commands delimiters */
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
			i = -1;                         /* hack incremet */
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
				i--;                    /* hack incremet */
		}

	/* collapse (command...(command...)...) or {command...{command...}...} */
	c = 0;                                          /* "recursive" level */
	c2 = 0;
	for (i = 0; int_buf[i]; i++)
		if (int_buf[i] == '(' || int_buf[i] == '{') {
			if (int_buf[i] == '(')
				c++;
			else
				c2++;
			collapse_pos(0, i + 1);
			i = -1;                         /* hack incremet */
		}
	for (i = 0; pos_buf[i] >= 0 && (c > 0 || c2 > 0); i++)
		if ((int_buf[i] == ')' && c > 0) || (int_buf[i] == '}' && c2 > 0)) {
			if (int_buf[i] == ')')
				c--;
			else
				c2--;
			collapse_pos(0, i + 1);
			i = -1;                         /* hack incremet */
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
			 && matchBuf[pos_buf[0]] == 'c'
			 && matchBuf[pos_buf[1]] == 'd'
			) {
				command_mode = FIND_DIR_ONLY;
			} else {
				command_mode = FIND_FILE_ONLY;
				break;
			}
		}
	for (i = 0; int_buf[i]; i++)
		/* "strlen" */;
	/* find last word */
	for (--i; i >= 0; i--) {
		c = int_buf[i];
		if (c == ' ' || c == '<' || c == '>' || c == '|' || c == '&') {
			collapse_pos(0, i + 1);
			break;
		}
	}
	/* skip first not quoted '\'' or '"' */
	for (i = 0; int_buf[i] == '\'' || int_buf[i] == '"'; i++)
		/*skip*/;
	/* collapse quote or unquote // or /~ */
	while ((int_buf[i] & ~QUOT) == '/'
	 && ((int_buf[i+1] & ~QUOT) == '/' || (int_buf[i+1] & ~QUOT) == '~')
	) {
		i++;
	}

	/* set only match and destroy quotes */
	j = 0;
	for (c = 0; pos_buf[i] >= 0; i++) {
		matchBuf[c++] = matchBuf[pos_buf[i]];
		j = pos_buf[i] + 1;
	}
	matchBuf[c] = '\0';
	/* old length matchBuf with quotes symbols */
	*len_with_quotes = j ? j - pos_buf[0] : 0;

	return command_mode;
#undef int_buf
#undef pos_buf
}

/*
 * display by column (original idea from ls applet,
 * very optimized by me :)
 */
static void showfiles(void)
{
	int ncols, row;
	int column_width = 0;
	int nfiles = num_matches;
	int nrows = nfiles;
	int l;

	/* find the longest file name-  use that as the column width */
	for (row = 0; row < nrows; row++) {
		l = strlen(matches[row]);
		if (column_width < l)
			column_width = l;
	}
	column_width += 2;              /* min space for columns */
	ncols = cmdedit_termw / column_width;

	if (ncols > 1) {
		nrows /= ncols;
		if (nfiles % ncols)
			nrows++;        /* round up fractionals */
	} else {
		ncols = 1;
	}
	for (row = 0; row < nrows; row++) {
		int n = row;
		int nc;

		for (nc = 1; nc < ncols && n+nrows < nfiles; n += nrows, nc++) {
			printf("%s%-*s", matches[n],
				(int)(column_width - strlen(matches[n])), "");
		}
		puts(matches[n]);
	}
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

/* Do TAB completion */
static void input_tab(smallint *lastWasTab)
{
	if (!(state->flags & TAB_COMPLETION))
		return;

	if (!*lastWasTab) {
		char *tmp, *tmp1;
		size_t len_found;
/*		char matchBuf[MAX_LINELEN]; */
#define matchBuf (S.input_tab__matchBuf)
		int find_type;
		int recalc_pos;

		*lastWasTab = TRUE;             /* flop trigger */

		/* Make a local copy of the string -- up
		 * to the position of the cursor */
		tmp = strncpy(matchBuf, command_ps, cursor);
		tmp[cursor] = '\0';

		find_type = find_match(matchBuf, &recalc_pos);

		/* Free up any memory already allocated */
		free_tab_completion_data();

#if ENABLE_FEATURE_USERNAME_COMPLETION
		/* If the word starts with `~' and there is no slash in the word,
		 * then try completing this word as a username. */
		if (state->flags & USERNAME_COMPLETION)
			if (matchBuf[0] == '~' && strchr(matchBuf, '/') == 0)
				username_tab_completion(matchBuf, NULL);
#endif
		/* Try to match any executable in our path and everything
		 * in the current working directory */
		if (!matches)
			exe_n_cwd_tab_completion(matchBuf, find_type);
		/* Sort, then remove any duplicates found */
		if (matches) {
			unsigned i;
			int n = 0;
			qsort_string_vector(matches, num_matches);
			for (i = 0; i < num_matches - 1; ++i) {
				if (matches[i] && matches[i+1]) { /* paranoia */
					if (strcmp(matches[i], matches[i+1]) == 0) {
						free(matches[i]);
						matches[i] = NULL; /* paranoia */
					} else {
						matches[n++] = matches[i];
					}
				}
			}
			matches[n] = matches[i];
			num_matches = n + 1;
		}
		/* Did we find exactly one match? */
		if (!matches || num_matches > 1) {
			beep();
			if (!matches)
				return;         /* not found */
			/* find minimal match */
			tmp1 = xstrdup(matches[0]);
			for (tmp = tmp1; *tmp; tmp++)
				for (len_found = 1; len_found < num_matches; len_found++)
					if (matches[len_found][(tmp - tmp1)] != *tmp) {
						*tmp = '\0';
						break;
					}
			if (*tmp1 == '\0') {        /* have unique */
				free(tmp1);
				return;
			}
			tmp = add_quote_for_spec_chars(tmp1);
			free(tmp1);
		} else {                        /* one match */
			tmp = add_quote_for_spec_chars(matches[0]);
			/* for next completion current found */
			*lastWasTab = FALSE;

			len_found = strlen(tmp);
			if (tmp[len_found-1] != '/') {
				tmp[len_found] = ' ';
				tmp[len_found+1] = '\0';
			}
		}
		len_found = strlen(tmp);
		/* have space to placed match? */
		if ((len_found - strlen(matchBuf) + command_len) < MAX_LINELEN) {
			/* before word for match   */
			command_ps[cursor - recalc_pos] = '\0';
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
			command_len = strlen(command_ps);
			/* write out the matched command   */
			redraw(cmdedit_y, command_len - recalc_pos);
		}
		free(tmp);
#undef matchBuf
	} else {
		/* Ok -- the last char was a TAB.  Since they
		 * just hit TAB again, print a list of all the
		 * available choices... */
		if (matches && num_matches > 0) {
			int sav_cursor = cursor;        /* change goto_new_line() */

			/* Go to the next line */
			goto_new_line();
			showfiles();
			redraw(0, command_len - sav_cursor);
		}
	}
}

#endif  /* FEATURE_COMMAND_TAB_COMPLETION */


line_input_t* FAST_FUNC new_line_input_t(int flags)
{
	line_input_t *n = xzalloc(sizeof(*n));
	n->flags = flags;
	return n;
}


#if MAX_HISTORY > 0

static void save_command_ps_at_cur_history(void)
{
	if (command_ps[0] != '\0') {
		int cur = state->cur_history;
		free(state->history[cur]);
		state->history[cur] = xstrdup(command_ps);
	}
}

/* state->flags is already checked to be nonzero */
static int get_previous_history(void)
{
	if ((state->flags & DO_HISTORY) && state->cur_history) {
		save_command_ps_at_cur_history();
		state->cur_history--;
		return 1;
	}
	beep();
	return 0;
}

static int get_next_history(void)
{
	if (state->flags & DO_HISTORY) {
		if (state->cur_history < state->cnt_history) {
			save_command_ps_at_cur_history(); /* save the current history line */
			return ++state->cur_history;
		}
	}
	beep();
	return 0;
}

#if ENABLE_FEATURE_EDITING_SAVEHISTORY
/* We try to ensure that concurrent additions to the history
 * do not overwrite each other.
 * Otherwise shell users get unhappy.
 *
 * History file is trimmed lazily, when it grows several times longer
 * than configured MAX_HISTORY lines.
 */

static void free_line_input_t(line_input_t *n)
{
	int i = n->cnt_history;
	while (i > 0)
		free(n->history[--i]);
	free(n);
}

/* state->flags is already checked to be nonzero */
static void load_history(line_input_t *st_parm)
{
	char *temp_h[MAX_HISTORY];
	char *line;
	FILE *fp;
	unsigned idx, i, line_len;

	/* NB: do not trash old history if file can't be opened */

	fp = fopen_for_read(st_parm->hist_file);
	if (fp) {
		/* clean up old history */
		for (idx = st_parm->cnt_history; idx > 0;) {
			idx--;
			free(st_parm->history[idx]);
			st_parm->history[idx] = NULL;
		}

		/* fill temp_h[], retaining only last MAX_HISTORY lines */
		memset(temp_h, 0, sizeof(temp_h));
		st_parm->cnt_history_in_file = idx = 0;
		while ((line = xmalloc_fgetline(fp)) != NULL) {
			if (line[0] == '\0') {
				free(line);
				continue;
			}
			free(temp_h[idx]);
			temp_h[idx] = line;
			st_parm->cnt_history_in_file++;
			idx++;
			if (idx == MAX_HISTORY)
				idx = 0;
		}
		fclose(fp);

		/* find first non-NULL temp_h[], if any */
		if (st_parm->cnt_history_in_file) {
			while (temp_h[idx] == NULL) {
				idx++;
				if (idx == MAX_HISTORY)
					idx = 0;
			}
		}

		/* copy temp_h[] to st_parm->history[] */
		for (i = 0; i < MAX_HISTORY;) {
			line = temp_h[idx];
			if (!line)
				break;
			idx++;
			if (idx == MAX_HISTORY)
				idx = 0;
			line_len = strlen(line);
			if (line_len >= MAX_LINELEN)
				line[MAX_LINELEN-1] = '\0';
			st_parm->history[i++] = line;
		}
		st_parm->cnt_history = i;
	}
}

/* state->flags is already checked to be nonzero */
static void save_history(char *str)
{
	int fd;
	int len, len2;

	fd = open(state->hist_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
	if (fd < 0)
		return;
	xlseek(fd, 0, SEEK_END); /* paranoia */
	len = strlen(str);
	str[len] = '\n'; /* we (try to) do atomic write */
	len2 = full_write(fd, str, len + 1);
	str[len] = '\0';
	close(fd);
	if (len2 != len + 1)
		return; /* "wtf?" */

	/* did we write so much that history file needs trimming? */
	state->cnt_history_in_file++;
	if (state->cnt_history_in_file > MAX_HISTORY * 4) {
		FILE *fp;
		char *new_name;
		line_input_t *st_temp;
		int i;

		/* we may have concurrently written entries from others.
		 * load them */
		st_temp = new_line_input_t(state->flags);
		st_temp->hist_file = state->hist_file;
		load_history(st_temp);

		/* write out temp file and replace hist_file atomically */
		new_name = xasprintf("%s.%u.new", state->hist_file, (int) getpid());
		fp = fopen_for_write(new_name);
		if (fp) {
			for (i = 0; i < st_temp->cnt_history; i++)
				fprintf(fp, "%s\n", st_temp->history[i]);
			fclose(fp);
			if (rename(new_name, state->hist_file) == 0)
				state->cnt_history_in_file = st_temp->cnt_history;
		}
		free(new_name);
		free_line_input_t(st_temp);
	}
}
#else
#define load_history(a) ((void)0)
#define save_history(a) ((void)0)
#endif /* FEATURE_COMMAND_SAVEHISTORY */

static void remember_in_history(char *str)
{
	int i;

	if (!(state->flags & DO_HISTORY))
		return;
	if (str[0] == '\0')
		return;
	i = state->cnt_history;
	/* Don't save dupes */
	if (i && strcmp(state->history[i-1], str) == 0)
		return;

	free(state->history[MAX_HISTORY]); /* redundant, paranoia */
	state->history[MAX_HISTORY] = NULL; /* redundant, paranoia */

	/* If history[] is full, remove the oldest command */
	/* we need to keep history[MAX_HISTORY] empty, hence >=, not > */
	if (i >= MAX_HISTORY) {
		free(state->history[0]);
		for (i = 0; i < MAX_HISTORY-1; i++)
			state->history[i] = state->history[i+1];
		/* i == MAX_HISTORY-1 */
	}
	/* i <= MAX_HISTORY-1 */
	state->history[i++] = xstrdup(str);
	/* i <= MAX_HISTORY */
	state->cur_history = i;
	state->cnt_history = i;
#if ENABLE_FEATURE_EDITING_SAVEHISTORY
	if ((state->flags & SAVE_HISTORY) && state->hist_file)
		save_history(str);
#endif
	USE_FEATURE_EDITING_FANCY_PROMPT(num_ok_lines++;)
}

#else /* MAX_HISTORY == 0 */
#define remember_in_history(a) ((void)0)
#endif /* MAX_HISTORY */


/*
 * This function is used to grab a character buffer
 * from the input file descriptor and allows you to
 * a string with full command editing (sort of like
 * a mini readline).
 *
 * The following standard commands are not implemented:
 * ESC-b -- Move back one word
 * ESC-f -- Move forward one word
 * ESC-d -- Delete back one word
 * ESC-h -- Delete forward one word
 * CTL-t -- Transpose two characters
 *
 * Minimalist vi-style command line editing available if configured.
 * vi mode implemented 2005 by Paul Fox <pgf@foxharp.boston.ma.us>
 */

#if ENABLE_FEATURE_EDITING_VI
static void
vi_Word_motion(char *command, int eat)
{
	while (cursor < command_len && !isspace(command[cursor]))
		input_forward();
	if (eat) while (cursor < command_len && isspace(command[cursor]))
		input_forward();
}

static void
vi_word_motion(char *command, int eat)
{
	if (isalnum(command[cursor]) || command[cursor] == '_') {
		while (cursor < command_len
		 && (isalnum(command[cursor+1]) || command[cursor+1] == '_'))
			input_forward();
	} else if (ispunct(command[cursor])) {
		while (cursor < command_len && ispunct(command[cursor+1]))
			input_forward();
	}

	if (cursor < command_len)
		input_forward();

	if (eat && cursor < command_len && isspace(command[cursor]))
		while (cursor < command_len && isspace(command[cursor]))
			input_forward();
}

static void
vi_End_motion(char *command)
{
	input_forward();
	while (cursor < command_len && isspace(command[cursor]))
		input_forward();
	while (cursor < command_len-1 && !isspace(command[cursor+1]))
		input_forward();
}

static void
vi_end_motion(char *command)
{
	if (cursor >= command_len-1)
		return;
	input_forward();
	while (cursor < command_len-1 && isspace(command[cursor]))
		input_forward();
	if (cursor >= command_len-1)
		return;
	if (isalnum(command[cursor]) || command[cursor] == '_') {
		while (cursor < command_len-1
		 && (isalnum(command[cursor+1]) || command[cursor+1] == '_')
		) {
			input_forward();
		}
	} else if (ispunct(command[cursor])) {
		while (cursor < command_len-1 && ispunct(command[cursor+1]))
			input_forward();
	}
}

static void
vi_Back_motion(char *command)
{
	while (cursor > 0 && isspace(command[cursor-1]))
		input_backward(1);
	while (cursor > 0 && !isspace(command[cursor-1]))
		input_backward(1);
}

static void
vi_back_motion(char *command)
{
	if (cursor <= 0)
		return;
	input_backward(1);
	while (cursor > 0 && isspace(command[cursor]))
		input_backward(1);
	if (cursor <= 0)
		return;
	if (isalnum(command[cursor]) || command[cursor] == '_') {
		while (cursor > 0
		 && (isalnum(command[cursor-1]) || command[cursor-1] == '_')
		) {
			input_backward(1);
		}
	} else if (ispunct(command[cursor])) {
		while (cursor > 0 && ispunct(command[cursor-1]))
			input_backward(1);
	}
}
#endif


/*
 * read_line_input and its helpers
 */

#if !ENABLE_FEATURE_EDITING_FANCY_PROMPT
static void parse_and_put_prompt(const char *prmt_ptr)
{
	cmdedit_prompt = prmt_ptr;
	cmdedit_prmt_len = strlen(prmt_ptr);
	put_prompt();
}
#else
static void parse_and_put_prompt(const char *prmt_ptr)
{
	int prmt_len = 0;
	size_t cur_prmt_len = 0;
	char flg_not_length = '[';
	char *prmt_mem_ptr = xzalloc(1);
	char *cwd_buf = xrealloc_getcwd_or_warn(NULL);
	char cbuf[2];
	char c;
	char *pbuf;

	cmdedit_prmt_len = 0;

	if (!cwd_buf) {
		cwd_buf = (char *)bb_msg_unknown;
	}

	cbuf[1] = '\0'; /* never changes */

	while (*prmt_ptr) {
		char *free_me = NULL;

		pbuf = cbuf;
		c = *prmt_ptr++;
		if (c == '\\') {
			const char *cp = prmt_ptr;
			int l;

			c = bb_process_escape_sequence(&prmt_ptr);
			if (prmt_ptr == cp) {
				if (*cp == '\0')
					break;
				c = *prmt_ptr++;

				switch (c) {
#if ENABLE_FEATURE_GETUSERNAME_AND_HOMEDIR
				case 'u':
					pbuf = user_buf ? user_buf : (char*)"";
					break;
#endif
				case 'h':
					pbuf = free_me = safe_gethostname();
					*strchrnul(pbuf, '.') = '\0';
					break;
				case '$':
					c = (geteuid() == 0 ? '#' : '$');
					break;
#if ENABLE_FEATURE_GETUSERNAME_AND_HOMEDIR
				case 'w':
					/* /home/user[/something] -> ~[/something] */
					pbuf = cwd_buf;
					l = strlen(home_pwd_buf);
					if (l != 0
					 && strncmp(home_pwd_buf, cwd_buf, l) == 0
					 && (cwd_buf[l]=='/' || cwd_buf[l]=='\0')
					 && strlen(cwd_buf + l) < PATH_MAX
					) {
						pbuf = free_me = xasprintf("~%s", cwd_buf + l);
					}
					break;
#endif
				case 'W':
					pbuf = cwd_buf;
					cp = strrchr(pbuf, '/');
					if (cp != NULL && cp != pbuf)
						pbuf += (cp-pbuf) + 1;
					break;
				case '!':
					pbuf = free_me = xasprintf("%d", num_ok_lines);
					break;
				case 'e': case 'E':     /* \e \E = \033 */
					c = '\033';
					break;
				case 'x': case 'X': {
					char buf2[4];
					for (l = 0; l < 3;) {
						unsigned h;
						buf2[l++] = *prmt_ptr;
						buf2[l] = '\0';
						h = strtoul(buf2, &pbuf, 16);
						if (h > UCHAR_MAX || (pbuf - buf2) < l) {
							buf2[--l] = '\0';
							break;
						}
						prmt_ptr++;
					}
					c = (char)strtoul(buf2, NULL, 16);
					if (c == 0)
						c = '?';
					pbuf = cbuf;
					break;
				}
				case '[': case ']':
					if (c == flg_not_length) {
						flg_not_length = (flg_not_length == '[' ? ']' : '[');
						continue;
					}
					break;
				} /* switch */
			} /* if */
		} /* if */
		cbuf[0] = c;
		cur_prmt_len = strlen(pbuf);
		prmt_len += cur_prmt_len;
		if (flg_not_length != ']')
			cmdedit_prmt_len += cur_prmt_len;
		prmt_mem_ptr = strcat(xrealloc(prmt_mem_ptr, prmt_len+1), pbuf);
		free(free_me);
	} /* while */

	if (cwd_buf != (char *)bb_msg_unknown)
		free(cwd_buf);
	cmdedit_prompt = prmt_mem_ptr;
	put_prompt();
}
#endif

static void cmdedit_setwidth(unsigned w, int redraw_flg)
{
	cmdedit_termw = w;
	if (redraw_flg) {
		/* new y for current cursor */
		int new_y = (cursor + cmdedit_prmt_len) / w;
		/* redraw */
		redraw((new_y >= cmdedit_y ? new_y : cmdedit_y), command_len - cursor);
		fflush(stdout);
	}
}

static void win_changed(int nsig)
{
	unsigned width;
	get_terminal_width_height(0, &width, NULL);
	cmdedit_setwidth(width, nsig /* - just a yes/no flag */);
	if (nsig == SIGWINCH)
		signal(SIGWINCH, win_changed); /* rearm ourself */
}

/*
 * The emacs and vi modes share much of the code in the big
 * command loop.  Commands entered when in vi's command mode (aka
 * "escape mode") get an extra bit added to distinguish them --
 * this keeps them from being self-inserted.  This clutters the
 * big switch a bit, but keeps all the code in one place.
 */

#define vbit 0x100

/* leave out the "vi-mode"-only case labels if vi editing isn't
 * configured. */
#define vi_case(caselabel) USE_FEATURE_EDITING(case caselabel)

/* convert uppercase ascii to equivalent control char, for readability */
#undef CTRL
#define CTRL(a) ((a) & ~0x40)

/* Returns:
 * -1 on read errors or EOF, or on bare Ctrl-D,
 * 0  on ctrl-C (the line entered is still returned in 'command'),
 * >0 length of input string, including terminating '\n'
 */
int FAST_FUNC read_line_input(const char *prompt, char *command, int maxsize, line_input_t *st)
{
	int len;
#if ENABLE_FEATURE_TAB_COMPLETION
	smallint lastWasTab = FALSE;
#endif
	unsigned ic;
	unsigned char c;
	smallint break_out = 0;
#if ENABLE_FEATURE_EDITING_VI
	smallint vi_cmdmode = 0;
	smalluint prevc;
#endif
	struct termios initial_settings;
	struct termios new_settings;

	INIT_S();

	if (tcgetattr(STDIN_FILENO, &initial_settings) < 0
	 || !(initial_settings.c_lflag & ECHO)
	) {
		/* Happens when e.g. stty -echo was run before */
		parse_and_put_prompt(prompt);
		fflush(stdout);
		if (fgets(command, maxsize, stdin) == NULL)
			len = -1; /* EOF or error */
		else
			len = strlen(command);
		DEINIT_S();
		return len;
	}

// FIXME: audit & improve this
	if (maxsize > MAX_LINELEN)
		maxsize = MAX_LINELEN;

	/* With null flags, no other fields are ever used */
	state = st ? st : (line_input_t*) &const_int_0;
#if ENABLE_FEATURE_EDITING_SAVEHISTORY
	if ((state->flags & SAVE_HISTORY) && state->hist_file)
		if (state->cnt_history == 0)
			load_history(state);
#endif
	if (state->flags & DO_HISTORY)
		state->cur_history = state->cnt_history;

	/* prepare before init handlers */
	cmdedit_y = 0;  /* quasireal y, not true if line > xt*yt */
	command_len = 0;
	command_ps = command;
	command[0] = '\0';

	new_settings = initial_settings;
	new_settings.c_lflag &= ~ICANON;        /* unbuffered input */
	/* Turn off echoing and CTRL-C, so we can trap it */
	new_settings.c_lflag &= ~(ECHO | ECHONL | ISIG);
	/* Hmm, in linux c_cc[] is not parsed if ICANON is off */
	new_settings.c_cc[VMIN] = 1;
	new_settings.c_cc[VTIME] = 0;
	/* Turn off CTRL-C, so we can trap it */
#ifndef _POSIX_VDISABLE
#define _POSIX_VDISABLE '\0'
#endif
	new_settings.c_cc[VINTR] = _POSIX_VDISABLE;
	tcsetattr_stdin_TCSANOW(&new_settings);

	/* Now initialize things */
	previous_SIGWINCH_handler = signal(SIGWINCH, win_changed);
	win_changed(0); /* do initial resizing */
#if ENABLE_FEATURE_GETUSERNAME_AND_HOMEDIR
	{
		struct passwd *entry;

		entry = getpwuid(geteuid());
		if (entry) {
			user_buf = xstrdup(entry->pw_name);
			home_pwd_buf = xstrdup(entry->pw_dir);
		}
	}
#endif

#if 0
	for (ic = 0; ic <= MAX_HISTORY; ic++)
		bb_error_msg("history[%d]:'%s'", ic, state->history[ic]);
	bb_error_msg("cur_history:%d cnt_history:%d", state->cur_history, state->cnt_history);
#endif

	/* Print out the command prompt */
	parse_and_put_prompt(prompt);

	while (1) {
		fflush(NULL);

		if (nonblock_safe_read(STDIN_FILENO, &c, 1) < 1) {
			/* if we can't read input then exit */
			goto prepare_to_die;
		}

		ic = c;

#if ENABLE_FEATURE_EDITING_VI
		newdelflag = 1;
		if (vi_cmdmode)
			ic |= vbit;
#endif
		switch (ic) {
		case '\n':
		case '\r':
		vi_case('\n'|vbit:)
		vi_case('\r'|vbit:)
			/* Enter */
			goto_new_line();
			break_out = 1;
			break;
		case CTRL('A'):
		vi_case('0'|vbit:)
			/* Control-a -- Beginning of line */
			input_backward(cursor);
			break;
		case CTRL('B'):
		vi_case('h'|vbit:)
		vi_case('\b'|vbit:)
		vi_case('\x7f'|vbit:) /* DEL */
			/* Control-b -- Move back one character */
			input_backward(1);
			break;
		case CTRL('C'):
		vi_case(CTRL('C')|vbit:)
			/* Control-c -- stop gathering input */
			goto_new_line();
			command_len = 0;
			break_out = -1; /* "do not append '\n'" */
			break;
		case CTRL('D'):
			/* Control-d -- Delete one character, or exit
			 * if the len=0 and no chars to delete */
			if (command_len == 0) {
				errno = 0;
 prepare_to_die:
				/* to control stopped jobs */
				break_out = command_len = -1;
				break;
			}
			input_delete(0);
			break;

		case CTRL('E'):
		vi_case('$'|vbit:)
			/* Control-e -- End of line */
			input_end();
			break;
		case CTRL('F'):
		vi_case('l'|vbit:)
		vi_case(' '|vbit:)
			/* Control-f -- Move forward one character */
			input_forward();
			break;

		case '\b':
		case '\x7f': /* DEL */
			/* Control-h and DEL */
			input_backspace();
			break;

#if ENABLE_FEATURE_TAB_COMPLETION
		case '\t':
			input_tab(&lastWasTab);
			break;
#endif

		case CTRL('K'):
			/* Control-k -- clear to end of line */
			command[cursor] = 0;
			command_len = cursor;
			printf("\033[J");
			break;
		case CTRL('L'):
		vi_case(CTRL('L')|vbit:)
			/* Control-l -- clear screen */
			printf("\033[H");
			redraw(0, command_len - cursor);
			break;

#if MAX_HISTORY > 0
		case CTRL('N'):
		vi_case(CTRL('N')|vbit:)
		vi_case('j'|vbit:)
			/* Control-n -- Get next command in history */
			if (get_next_history())
				goto rewrite_line;
			break;
		case CTRL('P'):
		vi_case(CTRL('P')|vbit:)
		vi_case('k'|vbit:)
			/* Control-p -- Get previous command from history */
			if (get_previous_history())
				goto rewrite_line;
			break;
#endif

		case CTRL('U'):
		vi_case(CTRL('U')|vbit:)
			/* Control-U -- Clear line before cursor */
			if (cursor) {
				overlapping_strcpy(command, command + cursor);
				command_len -= cursor;
				redraw(cmdedit_y, command_len);
			}
			break;
		case CTRL('W'):
		vi_case(CTRL('W')|vbit:)
			/* Control-W -- Remove the last word */
			while (cursor > 0 && isspace(command[cursor-1]))
				input_backspace();
			while (cursor > 0 && !isspace(command[cursor-1]))
				input_backspace();
			break;

#if ENABLE_FEATURE_EDITING_VI
		case 'i'|vbit:
			vi_cmdmode = 0;
			break;
		case 'I'|vbit:
			input_backward(cursor);
			vi_cmdmode = 0;
			break;
		case 'a'|vbit:
			input_forward();
			vi_cmdmode = 0;
			break;
		case 'A'|vbit:
			input_end();
			vi_cmdmode = 0;
			break;
		case 'x'|vbit:
			input_delete(1);
			break;
		case 'X'|vbit:
			if (cursor > 0) {
				input_backward(1);
				input_delete(1);
			}
			break;
		case 'W'|vbit:
			vi_Word_motion(command, 1);
			break;
		case 'w'|vbit:
			vi_word_motion(command, 1);
			break;
		case 'E'|vbit:
			vi_End_motion(command);
			break;
		case 'e'|vbit:
			vi_end_motion(command);
			break;
		case 'B'|vbit:
			vi_Back_motion(command);
			break;
		case 'b'|vbit:
			vi_back_motion(command);
			break;
		case 'C'|vbit:
			vi_cmdmode = 0;
			/* fall through */
		case 'D'|vbit:
			goto clear_to_eol;

		case 'c'|vbit:
			vi_cmdmode = 0;
			/* fall through */
		case 'd'|vbit: {
			int nc, sc;
			sc = cursor;
			prevc = ic;
			if (safe_read(STDIN_FILENO, &c, 1) < 1)
				goto prepare_to_die;
			if (c == (prevc & 0xff)) {
				/* "cc", "dd" */
				input_backward(cursor);
				goto clear_to_eol;
				break;
			}
			switch (c) {
			case 'w':
			case 'W':
			case 'e':
			case 'E':
				switch (c) {
				case 'w':   /* "dw", "cw" */
					vi_word_motion(command, vi_cmdmode);
					break;
				case 'W':   /* 'dW', 'cW' */
					vi_Word_motion(command, vi_cmdmode);
					break;
				case 'e':   /* 'de', 'ce' */
					vi_end_motion(command);
					input_forward();
					break;
				case 'E':   /* 'dE', 'cE' */
					vi_End_motion(command);
					input_forward();
					break;
				}
				nc = cursor;
				input_backward(cursor - sc);
				while (nc-- > cursor)
					input_delete(1);
				break;
			case 'b':  /* "db", "cb" */
			case 'B':  /* implemented as B */
				if (c == 'b')
					vi_back_motion(command);
				else
					vi_Back_motion(command);
				while (sc-- > cursor)
					input_delete(1);
				break;
			case ' ':  /* "d ", "c " */
				input_delete(1);
				break;
			case '$':  /* "d$", "c$" */
			clear_to_eol:
				while (cursor < command_len)
					input_delete(1);
				break;
			}
			break;
		}
		case 'p'|vbit:
			input_forward();
			/* fallthrough */
		case 'P'|vbit:
			put();
			break;
		case 'r'|vbit:
			if (safe_read(STDIN_FILENO, &c, 1) < 1)
				goto prepare_to_die;
			if (c == 0)
				beep();
			else {
				*(command + cursor) = c;
				bb_putchar(c);
				bb_putchar('\b');
			}
			break;
#endif /* FEATURE_COMMAND_EDITING_VI */

		case '\x1b': /* ESC */

#if ENABLE_FEATURE_EDITING_VI
			if (state->flags & VI_MODE) {
				/* ESC: insert mode --> command mode */
				vi_cmdmode = 1;
				input_backward(1);
				break;
			}
#endif
			/* escape sequence follows */
			if (safe_read(STDIN_FILENO, &c, 1) < 1)
				goto prepare_to_die;
			/* different vt100 emulations */
			if (c == '[' || c == 'O') {
		vi_case('['|vbit:)
		vi_case('O'|vbit:)
				if (safe_read(STDIN_FILENO, &c, 1) < 1)
					goto prepare_to_die;
			}
			if (c >= '1' && c <= '9') {
				unsigned char dummy;

				if (safe_read(STDIN_FILENO, &dummy, 1) < 1)
					goto prepare_to_die;
				if (dummy != '~')
					c = '\0';
			}

			switch (c) {
#if ENABLE_FEATURE_TAB_COMPLETION
			case '\t':                      /* Alt-Tab */
				input_tab(&lastWasTab);
				break;
#endif
#if MAX_HISTORY > 0
			case 'A':
				/* Up Arrow -- Get previous command from history */
				if (get_previous_history())
					goto rewrite_line;
				beep();
				break;
			case 'B':
				/* Down Arrow -- Get next command in history */
				if (!get_next_history())
					break;
 rewrite_line:
				/* Rewrite the line with the selected history item */
				/* change command */
				command_len = strlen(strcpy(command, state->history[state->cur_history] ? : ""));
				/* redraw and go to eol (bol, in vi */
				redraw(cmdedit_y, (state->flags & VI_MODE) ? 9999 : 0);
				break;
#endif
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
				input_delete(0);
				break;
			case '1': // vt100? linux vt? or what?
			case '7': // vt100? linux vt? or what?
			case 'H': /* xterm's <Home> */
				input_backward(cursor);
				break;
			case '4': // vt100? linux vt? or what?
			case '8': // vt100? linux vt? or what?
			case 'F': /* xterm's <End> */
				input_end();
				break;
			default:
				c = '\0';
				beep();
			}
			break;

		default:        /* If it's regular input, do the normal thing */

			/* Control-V -- force insert of next char */
			if (c == CTRL('V')) {
				if (safe_read(STDIN_FILENO, &c, 1) < 1)
					goto prepare_to_die;
				if (c == 0) {
					beep();
					break;
				}
			}

#if ENABLE_FEATURE_EDITING_VI
			if (vi_cmdmode)  /* Don't self-insert */
				break;
#endif
			if ((int)command_len >= (maxsize - 2))        /* Need to leave space for enter */
				break;

			command_len++;
			if (cursor == (command_len - 1)) {      /* Append if at the end of the line */
				command[cursor] = c;
				command[cursor+1] = '\0';
				cmdedit_set_out_char(' ');
			} else {                        /* Insert otherwise */
				int sc = cursor;

				memmove(command + sc + 1, command + sc, command_len - sc);
				command[sc] = c;
				sc++;
				/* rewrite from cursor */
				input_end();
				/* to prev x pos + 1 */
				input_backward(cursor - sc);
			}
			break;
		}
		if (break_out)                  /* Enter is the command terminator, no more input. */
			break;

#if ENABLE_FEATURE_TAB_COMPLETION
		if (c != '\t')
			lastWasTab = FALSE;
#endif
	}

	if (command_len > 0)
		remember_in_history(command);

	if (break_out > 0) {
		command[command_len++] = '\n';
		command[command_len] = '\0';
	}

#if ENABLE_FEATURE_TAB_COMPLETION
	free_tab_completion_data();
#endif

	/* restore initial_settings */
	tcsetattr_stdin_TCSANOW(&initial_settings);
	/* restore SIGWINCH handler */
	signal(SIGWINCH, previous_SIGWINCH_handler);
	fflush(stdout);

	len = command_len;
	DEINIT_S();

	return len; /* can't return command_len, DEINIT_S() destroys it */
}

#else

#undef read_line_input
int FAST_FUNC read_line_input(const char* prompt, char* command, int maxsize)
{
	fputs(prompt, stdout);
	fflush(stdout);
	fgets(command, maxsize, stdin);
	return strlen(command);
}

#endif  /* FEATURE_COMMAND_EDITING */


/*
 * Testing
 */

#ifdef TEST

#include <locale.h>

const char *applet_name = "debug stuff usage";

int main(int argc, char **argv)
{
	char buff[MAX_LINELEN];
	char *prompt =
#if ENABLE_FEATURE_EDITING_FANCY_PROMPT
		"\\[\\033[32;1m\\]\\u@\\[\\x1b[33;1m\\]\\h:"
		"\\[\\033[34;1m\\]\\w\\[\\033[35;1m\\] "
		"\\!\\[\\e[36;1m\\]\\$ \\[\\E[0m\\]";
#else
		"% ";
#endif

#if ENABLE_FEATURE_NONPRINTABLE_INVERSE_PUT
	setlocale(LC_ALL, "");
#endif
	while (1) {
		int l;
		l = read_line_input(prompt, buff);
		if (l <= 0 || buff[l-1] != '\n')
			break;
		buff[l-1] = 0;
		printf("*** read_line_input() returned line =%s=\n", buff);
	}
	printf("*** read_line_input() detect ^D\n");
	return 0;
}

#endif  /* TEST */
