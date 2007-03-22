/* vi: set sw=4 ts=4: */
/*
 * tiny vi.c: A small 'vi' clone
 * Copyright (C) 2000, 2001 Sterling Huxley <sterling@europa.com>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

/*
 * Things To Do:
 *	EXINIT
 *	$HOME/.exrc  and  ./.exrc
 *	add magic to search	/foo.*bar
 *	add :help command
 *	:map macros
 *	if mark[] values were line numbers rather than pointers
 *	   it would be easier to change the mark when add/delete lines
 *	More intelligence in refresh()
 *	":r !cmd"  and  "!cmd"  to filter text through an external command
 *	A true "undo" facility
 *	An "ex" line oriented mode- maybe using "cmdedit"
 */

#include "busybox.h"

#define ENABLE_FEATURE_VI_CRASHME 0

#if ENABLE_LOCALE_SUPPORT
#define Isprint(c) isprint((c))
#else
/* 0x9b is Meta-ESC */
#define Isprint(c) ((unsigned char)(c) >= ' ' && (c) != 0x7f && (unsigned char)(c) != 0x9b)
#endif

#define MAX_SCR_COLS		BUFSIZ

// Misc. non-Ascii keys that report an escape sequence
#define VI_K_UP			(char)128	// cursor key Up
#define VI_K_DOWN		(char)129	// cursor key Down
#define VI_K_RIGHT		(char)130	// Cursor Key Right
#define VI_K_LEFT		(char)131	// cursor key Left
#define VI_K_HOME		(char)132	// Cursor Key Home
#define VI_K_END		(char)133	// Cursor Key End
#define VI_K_INSERT		(char)134	// Cursor Key Insert
#define VI_K_PAGEUP		(char)135	// Cursor Key Page Up
#define VI_K_PAGEDOWN		(char)136	// Cursor Key Page Down
#define VI_K_FUN1		(char)137	// Function Key F1
#define VI_K_FUN2		(char)138	// Function Key F2
#define VI_K_FUN3		(char)139	// Function Key F3
#define VI_K_FUN4		(char)140	// Function Key F4
#define VI_K_FUN5		(char)141	// Function Key F5
#define VI_K_FUN6		(char)142	// Function Key F6
#define VI_K_FUN7		(char)143	// Function Key F7
#define VI_K_FUN8		(char)144	// Function Key F8
#define VI_K_FUN9		(char)145	// Function Key F9
#define VI_K_FUN10		(char)146	// Function Key F10
#define VI_K_FUN11		(char)147	// Function Key F11
#define VI_K_FUN12		(char)148	// Function Key F12

/* vt102 typical ESC sequence */
/* terminal standout start/normal ESC sequence */
static const char SOs[] = "\033[7m";
static const char SOn[] = "\033[0m";
/* terminal bell sequence */
static const char bell[] = "\007";
/* Clear-end-of-line and Clear-end-of-screen ESC sequence */
static const char Ceol[] = "\033[0K";
static const char Ceos [] = "\033[0J";
/* Cursor motion arbitrary destination ESC sequence */
static const char CMrc[] = "\033[%d;%dH";
/* Cursor motion up and down ESC sequence */
static const char CMup[] = "\033[A";
static const char CMdown[] = "\n";


enum {
	YANKONLY = FALSE,
	YANKDEL = TRUE,
	FORWARD = 1,	// code depends on "1"  for array index
	BACK = -1,	// code depends on "-1" for array index
	LIMITED = 0,	// how much of text[] in char_search
	FULL = 1,	// how much of text[] in char_search

	S_BEFORE_WS = 1,	// used in skip_thing() for moving "dot"
	S_TO_WS = 2,		// used in skip_thing() for moving "dot"
	S_OVER_WS = 3,		// used in skip_thing() for moving "dot"
	S_END_PUNCT = 4,	// used in skip_thing() for moving "dot"
	S_END_ALNUM = 5,	// used in skip_thing() for moving "dot"
};

/* vi.c expects chars to be unsigned. */
/* busybox build system provides that, but it's better */
/* to audit and fix the source */

static int vi_setops;
#define VI_AUTOINDENT 1
#define VI_SHOWMATCH  2
#define VI_IGNORECASE 4
#define VI_ERR_METHOD 8
#define autoindent (vi_setops & VI_AUTOINDENT)
#define showmatch  (vi_setops & VI_SHOWMATCH )
#define ignorecase (vi_setops & VI_IGNORECASE)
/* indicate error with beep or flash */
#define err_method (vi_setops & VI_ERR_METHOD)


static int editing;		// >0 while we are editing a file
static int cmd_mode;		// 0=command  1=insert 2=replace
static int file_modified;	// buffer contents changed
static int last_file_modified = -1;
static int fn_start;		// index of first cmd line file name
static int save_argc;		// how many file names on cmd line
static int cmdcnt;		// repetition count
static fd_set rfds;		// use select() for small sleeps
static struct timeval tv;	// use select() for small sleeps
static int rows, columns;	// the terminal screen is this size
static int crow, ccol, offset;	// cursor is on Crow x Ccol with Horz Ofset
static char *status_buffer;	// mesages to the user
#define STATUS_BUFFER_LEN  200
static int have_status_msg;     // is default edit status needed?
static int last_status_cksum;   // hash of current status line
static char *cfn;		// previous, current, and next file name
static char *text, *end;	// pointers to the user data in memory
static char *screen;		// pointer to the virtual screen buffer
static int screensize;		//            and its size
static char *screenbegin;	// index into text[], of top line on the screen
static char *dot;		// where all the action takes place
static int tabstop;
static struct termios term_orig, term_vi;	// remember what the cooked mode was
static char erase_char;         // the users erase character
static char last_input_char;    // last char read from user
static char last_forward_char;  // last char searched for with 'f'

#if ENABLE_FEATURE_VI_OPTIMIZE_CURSOR
static int last_row;		// where the cursor was last moved to
#endif
#if ENABLE_FEATURE_VI_USE_SIGNALS
static jmp_buf restart;		// catch_sig()
#endif
#if ENABLE_FEATURE_VI_USE_SIGNALS || ENABLE_FEATURE_VI_CRASHME
static int my_pid;
#endif
#if ENABLE_FEATURE_VI_DOT_CMD
static int adding2q;		// are we currently adding user input to q
static char *last_modifying_cmd;	// last modifying cmd for "."
static char *ioq, *ioq_start;	// pointer to string for get_one_char to "read"
#endif
#if ENABLE_FEATURE_VI_DOT_CMD || ENABLE_FEATURE_VI_YANKMARK
static char *modifying_cmds;	// cmds that modify text[]
#endif
#if ENABLE_FEATURE_VI_READONLY
static int vi_readonly, readonly;
#endif
#if ENABLE_FEATURE_VI_YANKMARK
static char *reg[28];		// named register a-z, "D", and "U" 0-25,26,27
static int YDreg, Ureg;		// default delete register and orig line for "U"
static char *mark[28];		// user marks points somewhere in text[]-  a-z and previous context ''
static char *context_start, *context_end;
#endif
#if ENABLE_FEATURE_VI_SEARCH
static char *last_search_pattern;	// last pattern from a '/' or '?' search
#endif


static void edit_file(char *);	// edit one file
static void do_cmd(char);	// execute a command
static void sync_cursor(char *, int *, int *);	// synchronize the screen cursor to dot
static char *begin_line(char *);	// return pointer to cur line B-o-l
static char *end_line(char *);	// return pointer to cur line E-o-l
static char *prev_line(char *);	// return pointer to prev line B-o-l
static char *next_line(char *);	// return pointer to next line B-o-l
static char *end_screen(void);	// get pointer to last char on screen
static int count_lines(char *, char *);	// count line from start to stop
static char *find_line(int);	// find begining of line #li
static char *move_to_col(char *, int);	// move "p" to column l
static int isblnk(char);	// is the char a blank or tab
static void dot_left(void);	// move dot left- dont leave line
static void dot_right(void);	// move dot right- dont leave line
static void dot_begin(void);	// move dot to B-o-l
static void dot_end(void);	// move dot to E-o-l
static void dot_next(void);	// move dot to next line B-o-l
static void dot_prev(void);	// move dot to prev line B-o-l
static void dot_scroll(int, int);	// move the screen up or down
static void dot_skip_over_ws(void);	// move dot pat WS
static void dot_delete(void);	// delete the char at 'dot'
static char *bound_dot(char *);	// make sure  text[0] <= P < "end"
static char *new_screen(int, int);	// malloc virtual screen memory
static char *new_text(int);	// malloc memory for text[] buffer
static char *char_insert(char *, char);	// insert the char c at 'p'
static char *stupid_insert(char *, char);	// stupidly insert the char c at 'p'
static char find_range(char **, char **, char);	// return pointers for an object
static int st_test(char *, int, int, char *);	// helper for skip_thing()
static char *skip_thing(char *, int, int, int);	// skip some object
static char *find_pair(char *, char);	// find matching pair ()  []  {}
static char *text_hole_delete(char *, char *);	// at "p", delete a 'size' byte hole
static char *text_hole_make(char *, int);	// at "p", make a 'size' byte hole
static char *yank_delete(char *, char *, int, int);	// yank text[] into register then delete
static void show_help(void);	// display some help info
static void rawmode(void);	// set "raw" mode on tty
static void cookmode(void);	// return to "cooked" mode on tty
static int mysleep(int);	// sleep for 'h' 1/100 seconds
static char readit(void);	// read (maybe cursor) key from stdin
static char get_one_char(void);	// read 1 char from stdin
static int file_size(const char *);   // what is the byte size of "fn"
static int file_insert(char *, char *, int);
static int file_write(char *, char *, char *);
static void place_cursor(int, int, int);
static void screen_erase(void);
static void clear_to_eol(void);
static void clear_to_eos(void);
static void standout_start(void);	// send "start reverse video" sequence
static void standout_end(void);	// send "end reverse video" sequence
static void flash(int);		// flash the terminal screen
static void show_status_line(void);	// put a message on the bottom line
static void psb(const char *, ...);     // Print Status Buf
static void psbs(const char *, ...);    // Print Status Buf in standout mode
static void ni(const char *);		// display messages
static int format_edit_status(void);	// format file status on status line
static void redraw(int);	// force a full screen refresh
static void format_line(char*, char*, int);
static void refresh(int);	// update the terminal from screen[]

static void Indicate_Error(void);       // use flash or beep to indicate error
#define indicate_error(c) Indicate_Error()
static void Hit_Return(void);

#if ENABLE_FEATURE_VI_SEARCH
static char *char_search(char *, const char *, int, int);	// search for pattern starting at p
static int mycmp(const char *, const char *, int);	// string cmp based in "ignorecase"
#endif
#if ENABLE_FEATURE_VI_COLON
static char *get_one_address(char *, int *);	// get colon addr, if present
static char *get_address(char *, int *, int *);	// get two colon addrs, if present
static void colon(char *);	// execute the "colon" mode cmds
#endif
#if ENABLE_FEATURE_VI_USE_SIGNALS
static void winch_sig(int);	// catch window size changes
static void suspend_sig(int);	// catch ctrl-Z
static void catch_sig(int);     // catch ctrl-C and alarm time-outs
#endif
#if ENABLE_FEATURE_VI_DOT_CMD
static void start_new_cmd_q(char);	// new queue for command
static void end_cmd_q(void);	// stop saving input chars
#else
#define end_cmd_q() ((void)0)
#endif
#if ENABLE_FEATURE_VI_SETOPTS
static void showmatching(char *);	// show the matching pair ()  []  {}
#endif
#if ENABLE_FEATURE_VI_YANKMARK || (ENABLE_FEATURE_VI_COLON && ENABLE_FEATURE_VI_SEARCH) || ENABLE_FEATURE_VI_CRASHME
static char *string_insert(char *, char *);	// insert the string at 'p'
#endif
#if ENABLE_FEATURE_VI_YANKMARK
static char *text_yank(char *, char *, int);	// save copy of "p" into a register
static char what_reg(void);		// what is letter of current YDreg
static void check_context(char);	// remember context for '' command
#endif
#if ENABLE_FEATURE_VI_CRASHME
static void crash_dummy();
static void crash_test();
static int crashme = 0;
#endif
#if ENABLE_FEATURE_VI_COLON
static char *initial_cmds[] = { NULL, NULL , NULL }; // currently 2 entries, NULL terminated
#endif


static void write1(const char *out)
{
	fputs(out, stdout);
}

int vi_main(int argc, char **argv);
int vi_main(int argc, char **argv)
{
	int c;
	RESERVE_CONFIG_BUFFER(STATUS_BUFFER, STATUS_BUFFER_LEN);

#if ENABLE_FEATURE_VI_YANKMARK
	int i;
#endif
#if defined(CONFIG_FEATURE_VI_USE_SIGNALS) || defined(CONFIG_FEATURE_VI_CRASHME)
	my_pid = getpid();
#endif
#if ENABLE_FEATURE_VI_CRASHME
	srand((long) my_pid);
#endif

	status_buffer = STATUS_BUFFER;
	last_status_cksum = 0;

#if ENABLE_FEATURE_VI_READONLY
	vi_readonly = readonly = FALSE;
	if (strncmp(argv[0], "view", 4) == 0) {
		readonly = TRUE;
		vi_readonly = TRUE;
	}
#endif
	vi_setops = VI_AUTOINDENT | VI_SHOWMATCH | VI_IGNORECASE | VI_ERR_METHOD;
#if ENABLE_FEATURE_VI_YANKMARK
	for (i = 0; i < 28; i++) {
		reg[i] = 0;
	}					// init the yank regs
#endif
#if ENABLE_FEATURE_VI_DOT_CMD || ENABLE_FEATURE_VI_YANKMARK
	modifying_cmds = (char *) "aAcCdDiIJoOpPrRsxX<>~";	// cmds modifying text[]
#endif

	//  1-  process $HOME/.exrc file (not inplemented yet)
	//  2-  process EXINIT variable from environment
	//  3-  process command line args
#if ENABLE_FEATURE_VI_COLON
	{
		char *p = getenv("EXINIT");
		if (p && *p)
			initial_cmds[0] = xstrdup(p);
	}
#endif
	while ((c = getopt(argc, argv, "hCR" USE_FEATURE_VI_COLON("c:"))) != -1) {
		switch (c) {
#if ENABLE_FEATURE_VI_CRASHME
		case 'C':
			crashme = 1;
			break;
#endif
#if ENABLE_FEATURE_VI_READONLY
		case 'R':		// Read-only flag
			readonly = TRUE;
			vi_readonly = TRUE;
			break;
#endif
			//case 'r':	// recover flag-  ignore- we don't use tmp file
			//case 'x':	// encryption flag- ignore
			//case 'c':	// execute command first
#if ENABLE_FEATURE_VI_COLON
		case 'c':		// cmd line vi command
			if (*optarg)
				initial_cmds[initial_cmds[0] != 0] = xstrdup(optarg);
			break;
			//case 'h':	// help -- just use default
#endif
		default:
			show_help();
			return 1;
		}
	}

	// The argv array can be used by the ":next"  and ":rewind" commands
	// save optind.
	fn_start = optind;	// remember first file name for :next and :rew
	save_argc = argc;

	//----- This is the main file handling loop --------------
	if (optind >= argc) {
		editing = 1;	// 0= exit,  1= one file,  2= multiple files
		edit_file(0);
	} else {
		for (; optind < argc; optind++) {
			editing = 1;	// 0=exit, 1=one file, 2+ =many files
			free(cfn);
			cfn = xstrdup(argv[optind]);
			edit_file(cfn);
		}
	}
	//-----------------------------------------------------------

	return 0;
}

static void edit_file(char * fn)
{
	char c;
	int cnt, size, ch;

#if ENABLE_FEATURE_VI_USE_SIGNALS
	int sig;
#endif
#if ENABLE_FEATURE_VI_YANKMARK
	static char *cur_line;
#endif

	rawmode();
	rows = 24;
	columns = 80;
	ch = -1;
	if (ENABLE_FEATURE_VI_WIN_RESIZE)
		get_terminal_width_height(0, &columns, &rows);
	new_screen(rows, columns);	// get memory for virtual screen

	cnt = file_size(fn);	// file size
	size = 2 * cnt;		// 200% of file size
	new_text(size);		// get a text[] buffer
	screenbegin = dot = end = text;
	if (fn != 0) {
		ch = file_insert(fn, text, cnt);
	}
	if (ch < 1) {
		char_insert(text, '\n');	// start empty buf with dummy line
	}
	file_modified = 0;
	last_file_modified = -1;
#if ENABLE_FEATURE_VI_YANKMARK
	YDreg = 26;			// default Yank/Delete reg
	Ureg = 27;			// hold orig line for "U" cmd
	for (cnt = 0; cnt < 28; cnt++) {
		mark[cnt] = 0;
	}					// init the marks
	mark[26] = mark[27] = text;	// init "previous context"
#endif

	last_forward_char = last_input_char = '\0';
	crow = 0;
	ccol = 0;

#if ENABLE_FEATURE_VI_USE_SIGNALS
	catch_sig(0);
	signal(SIGWINCH, winch_sig);
	signal(SIGTSTP, suspend_sig);
	sig = setjmp(restart);
	if (sig != 0) {
		screenbegin = dot = text;
	}
#endif

	editing = 1;
	cmd_mode = 0;		// 0=command  1=insert  2='R'eplace
	cmdcnt = 0;
	tabstop = 8;
	offset = 0;			// no horizontal offset
	c = '\0';
#if ENABLE_FEATURE_VI_DOT_CMD
	free(last_modifying_cmd);
	free(ioq_start);
	ioq = ioq_start = last_modifying_cmd = NULL;
	adding2q = 0;
#endif
	redraw(FALSE);			// dont force every col re-draw
	show_status_line();

#if ENABLE_FEATURE_VI_COLON
	{
		char *p, *q;
		int n = 0;

		while ((p = initial_cmds[n])) {
			do {
				q = p;
				p = strchr(q,'\n');
				if (p)
					while(*p == '\n')
						*p++ = '\0';
				if (*q)
					colon(q);
			} while (p);
			free(initial_cmds[n]);
			initial_cmds[n] = NULL;
			n++;
		}
	}
#endif
	//------This is the main Vi cmd handling loop -----------------------
	while (editing > 0) {
#if ENABLE_FEATURE_VI_CRASHME
		if (crashme > 0) {
			if ((end - text) > 1) {
				crash_dummy();	// generate a random command
			} else {
				crashme = 0;
				dot = string_insert(text, "\n\n#####  Ran out of text to work on.  #####\n\n"); // insert the string
				refresh(FALSE);
			}
		}
#endif
		last_input_char = c = get_one_char();	// get a cmd from user
#if ENABLE_FEATURE_VI_YANKMARK
		// save a copy of the current line- for the 'U" command
		if (begin_line(dot) != cur_line) {
			cur_line = begin_line(dot);
			text_yank(begin_line(dot), end_line(dot), Ureg);
		}
#endif
#if ENABLE_FEATURE_VI_DOT_CMD
		// These are commands that change text[].
		// Remember the input for the "." command
		if (!adding2q && ioq_start == 0
		 && strchr(modifying_cmds, c)
		) {
			start_new_cmd_q(c);
		}
#endif
		do_cmd(c);		// execute the user command
		//
		// poll to see if there is input already waiting. if we are
		// not able to display output fast enough to keep up, skip
		// the display update until we catch up with input.
		if (mysleep(0) == 0) {
			// no input pending- so update output
			refresh(FALSE);
			show_status_line();
		}
#if ENABLE_FEATURE_VI_CRASHME
		if (crashme > 0)
			crash_test();	// test editor variables
#endif
	}
	//-------------------------------------------------------------------

	place_cursor(rows, 0, FALSE);	// go to bottom of screen
	clear_to_eol();		// Erase to end of line
	cookmode();
}

//----- The Colon commands -------------------------------------
#if ENABLE_FEATURE_VI_COLON
static char *get_one_address(char * p, int *addr)	// get colon addr, if present
{
	int st;
	char *q;

#if ENABLE_FEATURE_VI_YANKMARK
	char c;
#endif
#if ENABLE_FEATURE_VI_SEARCH
	char *pat, buf[BUFSIZ];
#endif

	*addr = -1;			// assume no addr
	if (*p == '.') {	// the current line
		p++;
		q = begin_line(dot);
		*addr = count_lines(text, q);
#if ENABLE_FEATURE_VI_YANKMARK
	} else if (*p == '\'') {	// is this a mark addr
		p++;
		c = tolower(*p);
		p++;
		if (c >= 'a' && c <= 'z') {
			// we have a mark
			c = c - 'a';
			q = mark[(unsigned char) c];
			if (q != NULL) {	// is mark valid
				*addr = count_lines(text, q);	// count lines
			}
		}
#endif
#if ENABLE_FEATURE_VI_SEARCH
	} else if (*p == '/') {	// a search pattern
		q = buf;
		for (p++; *p; p++) {
			if (*p == '/')
				break;
			*q++ = *p;
			*q = '\0';
		}
		pat = xstrdup(buf);	// save copy of pattern
		if (*p == '/')
			p++;
		q = char_search(dot, pat, FORWARD, FULL);
		if (q != NULL) {
			*addr = count_lines(text, q);
		}
		free(pat);
#endif
	} else if (*p == '$') {	// the last line in file
		p++;
		q = begin_line(end - 1);
		*addr = count_lines(text, q);
	} else if (isdigit(*p)) {	// specific line number
		sscanf(p, "%d%n", addr, &st);
		p += st;
	} else {			// I don't reconise this
		// unrecognised address- assume -1
		*addr = -1;
	}
	return p;
}

static char *get_address(char *p, int *b, int *e)	// get two colon addrs, if present
{
	//----- get the address' i.e., 1,3   'a,'b  -----
	// get FIRST addr, if present
	while (isblnk(*p))
		p++;				// skip over leading spaces
	if (*p == '%') {			// alias for 1,$
		p++;
		*b = 1;
		*e = count_lines(text, end-1);
		goto ga0;
	}
	p = get_one_address(p, b);
	while (isblnk(*p))
		p++;
	if (*p == ',') {			// is there a address separator
		p++;
		while (isblnk(*p))
			p++;
		// get SECOND addr, if present
		p = get_one_address(p, e);
	}
 ga0:
	while (isblnk(*p))
		p++;				// skip over trailing spaces
	return p;
}

#if ENABLE_FEATURE_VI_SET && ENABLE_FEATURE_VI_SETOPTS
static void setops(const char *args, const char *opname, int flg_no,
			const char *short_opname, int opt)
{
	const char *a = args + flg_no;
	int l = strlen(opname) - 1; /* opname have + ' ' */

	if (strncasecmp(a, opname, l) == 0
	 || strncasecmp(a, short_opname, 2) == 0
	) {
		if (flg_no)
			vi_setops &= ~opt;
		else
			vi_setops |= opt;
	}
}
#endif

static void colon(char * buf)
{
	char c, *orig_buf, *buf1, *q, *r;
	char *fn, cmd[BUFSIZ], args[BUFSIZ];
	int i, l, li, ch, b, e;
	int useforce = FALSE, forced = FALSE;
	struct stat st_buf;

	// :3154	// if (-e line 3154) goto it  else stay put
	// :4,33w! foo	// write a portion of buffer to file "foo"
	// :w		// write all of buffer to current file
	// :q		// quit
	// :q!		// quit- dont care about modified file
	// :'a,'z!sort -u   // filter block through sort
	// :'f		// goto mark "f"
	// :'fl		// list literal the mark "f" line
	// :.r bar	// read file "bar" into buffer before dot
	// :/123/,/abc/d    // delete lines from "123" line to "abc" line
	// :/xyz/	// goto the "xyz" line
	// :s/find/replace/ // substitute pattern "find" with "replace"
	// :!<cmd>	// run <cmd> then return
	//

	if (!buf[0])
		goto vc1;
	if (*buf == ':')
		buf++;			// move past the ':'

	li = ch = i = 0;
	b = e = -1;
	q = text;			// assume 1,$ for the range
	r = end - 1;
	li = count_lines(text, end - 1);
	fn = cfn;			// default to current file
	memset(cmd, '\0', BUFSIZ);	// clear cmd[]
	memset(args, '\0', BUFSIZ);	// clear args[]

	// look for optional address(es)  :.  :1  :1,9   :'q,'a   :%
	buf = get_address(buf, &b, &e);

	// remember orig command line
	orig_buf = buf;

	// get the COMMAND into cmd[]
	buf1 = cmd;
	while (*buf != '\0') {
		if (isspace(*buf))
			break;
		*buf1++ = *buf++;
	}
	// get any ARGuments
	while (isblnk(*buf))
		buf++;
	strcpy(args, buf);
	buf1 = last_char_is(cmd, '!');
	if (buf1) {
		useforce = TRUE;
		*buf1 = '\0';   // get rid of !
	}
	if (b >= 0) {
		// if there is only one addr, then the addr
		// is the line number of the single line the
		// user wants. So, reset the end
		// pointer to point at end of the "b" line
		q = find_line(b);	// what line is #b
		r = end_line(q);
		li = 1;
	}
	if (e >= 0) {
		// we were given two addrs.  change the
		// end pointer to the addr given by user.
		r = find_line(e);	// what line is #e
		r = end_line(r);
		li = e - b + 1;
	}
	// ------------ now look for the command ------------
	i = strlen(cmd);
	if (i == 0) {		// :123CR goto line #123
		if (b >= 0) {
			dot = find_line(b);	// what line is #b
			dot_skip_over_ws();
		}
	}
#if ENABLE_FEATURE_ALLOW_EXEC
	else if (strncmp(cmd, "!", 1) == 0) {	// run a cmd
		// :!ls   run the <cmd>
		alarm(0);		// wait for input- no alarms
		place_cursor(rows - 1, 0, FALSE);	// go to Status line
		clear_to_eol();			// clear the line
		cookmode();
		system(orig_buf + 1);		// run the cmd
		rawmode();
		Hit_Return();			// let user see results
		alarm(3);		// done waiting for input
	}
#endif
	else if (strncmp(cmd, "=", i) == 0) {	// where is the address
		if (b < 0) {	// no addr given- use defaults
			b = e = count_lines(text, dot);
		}
		psb("%d", b);
	} else if (strncasecmp(cmd, "delete", i) == 0) {	// delete lines
		if (b < 0) {	// no addr given- use defaults
			q = begin_line(dot);	// assume .,. for the range
			r = end_line(dot);
		}
		dot = yank_delete(q, r, 1, YANKDEL);	// save, then delete lines
		dot_skip_over_ws();
	} else if (strncasecmp(cmd, "edit", i) == 0) {	// Edit a file
		int sr;
		sr= 0;
		// don't edit, if the current file has been modified
		if (file_modified && ! useforce) {
			psbs("No write since last change (:edit! overrides)");
			goto vc1;
		}
		if (args[0]) {
			// the user supplied a file name
			fn= args;
		} else if (cfn && cfn[0]) {
			// no user supplied name- use the current filename
			fn= cfn;
			goto vc5;
		} else {
			// no user file name, no current name- punt
			psbs("No current filename");
			goto vc1;
		}

		// see if file exists- if not, its just a new file request
		sr = stat(fn, &st_buf);
		if (sr < 0) {
			// This is just a request for a new file creation.
			// The file_insert below will fail but we get
			// an empty buffer with a file name.  Then the "write"
			// command can do the create.
		} else {
			if ((st_buf.st_mode & S_IFREG) == 0) {
				// This is not a regular file
				psbs("\"%s\" is not a regular file", fn);
				goto vc1;
			}
			if ((st_buf.st_mode & (S_IRUSR | S_IRGRP | S_IROTH)) == 0) {
				// dont have any read permissions
				psbs("\"%s\" is not readable", fn);
				goto vc1;
			}
		}

		// There is a read-able regular file
		// make this the current file
		q = xstrdup(fn);	// save the cfn
		free(cfn);		// free the old name
		cfn = q;			// remember new cfn

 vc5:
		// delete all the contents of text[]
		new_text(2 * file_size(fn));
		screenbegin = dot = end = text;

		// insert new file
		ch = file_insert(fn, text, file_size(fn));

		if (ch < 1) {
			// start empty buf with dummy line
			char_insert(text, '\n');
			ch = 1;
		}
		file_modified = 0;
		last_file_modified = -1;
#if ENABLE_FEATURE_VI_YANKMARK
		if (Ureg >= 0 && Ureg < 28 && reg[Ureg] != 0) {
			free(reg[Ureg]);	//   free orig line reg- for 'U'
			reg[Ureg]= 0;
		}
		if (YDreg >= 0 && YDreg < 28 && reg[YDreg] != 0) {
			free(reg[YDreg]);	//   free default yank/delete register
			reg[YDreg]= 0;
		}
		for (li = 0; li < 28; li++) {
			mark[li] = 0;
		}				// init the marks
#endif
		// how many lines in text[]?
		li = count_lines(text, end - 1);
		psb("\"%s\"%s"
#if ENABLE_FEATURE_VI_READONLY
			"%s"
#endif
			" %dL, %dC", cfn,
			(sr < 0 ? " [New file]" : ""),
#if ENABLE_FEATURE_VI_READONLY
			((vi_readonly || readonly) ? " [Read only]" : ""),
#endif
			li, ch);
	} else if (strncasecmp(cmd, "file", i) == 0) {	// what File is this
		if (b != -1 || e != -1) {
			ni("No address allowed on this command");
			goto vc1;
		}
		if (args[0]) {
			// user wants a new filename
			free(cfn);
			cfn = xstrdup(args);
		} else {
			// user wants file status info
			last_status_cksum = 0;	// force status update
		}
	} else if (strncasecmp(cmd, "features", i) == 0) {	// what features are available
		// print out values of all features
		place_cursor(rows - 1, 0, FALSE);	// go to Status line, bottom of screen
		clear_to_eol();	// clear the line
		cookmode();
		show_help();
		rawmode();
		Hit_Return();
	} else if (strncasecmp(cmd, "list", i) == 0) {	// literal print line
		if (b < 0) {	// no addr given- use defaults
			q = begin_line(dot);	// assume .,. for the range
			r = end_line(dot);
		}
		place_cursor(rows - 1, 0, FALSE);	// go to Status line, bottom of screen
		clear_to_eol();	// clear the line
		puts("\r");
		for (; q <= r; q++) {
			int c_is_no_print;

			c = *q;
			c_is_no_print = (c & 0x80) && !Isprint(c);
			if (c_is_no_print) {
				c = '.';
				standout_start();
				}
			if (c == '\n') {
				write1("$\r");
			} else if (c < ' ' || c == 127) {
				putchar('^');
				if (c == 127)
					c = '?';
				else
					c += '@';
			}
			putchar(c);
			if (c_is_no_print)
				standout_end();
		}
#if ENABLE_FEATURE_VI_SET
 vc2:
#endif
		Hit_Return();
	} else if (strncasecmp(cmd, "quit", i) == 0 // Quit
	        || strncasecmp(cmd, "next", i) == 0 // edit next file
	) {
		if (useforce) {
			// force end of argv list
			if (*cmd == 'q') {
				optind = save_argc;
			}
			editing = 0;
			goto vc1;
		}
		// don't exit if the file been modified
		if (file_modified) {
			psbs("No write since last change (:%s! overrides)",
				 (*cmd == 'q' ? "quit" : "next"));
			goto vc1;
		}
		// are there other file to edit
		if (*cmd == 'q' && optind < save_argc - 1) {
			psbs("%d more file to edit", (save_argc - optind - 1));
			goto vc1;
		}
		if (*cmd == 'n' && optind >= save_argc - 1) {
			psbs("No more files to edit");
			goto vc1;
		}
		editing = 0;
	} else if (strncasecmp(cmd, "read", i) == 0) {	// read file into text[]
		fn = args;
		if (!fn[0]) {
			psbs("No filename given");
			goto vc1;
		}
		if (b < 0) {	// no addr given- use defaults
			q = begin_line(dot);	// assume "dot"
		}
		// read after current line- unless user said ":0r foo"
		if (b != 0)
			q = next_line(q);
#if ENABLE_FEATURE_VI_READONLY
		l = readonly;			// remember current files' status
#endif
		ch = file_insert(fn, q, file_size(fn));
#if ENABLE_FEATURE_VI_READONLY
		readonly= l;
#endif
		if (ch < 0)
			goto vc1;	// nothing was inserted
		// how many lines in text[]?
		li = count_lines(q, q + ch - 1);
		psb("\"%s\""
#if ENABLE_FEATURE_VI_READONLY
			"%s"
#endif
			" %dL, %dC", fn,
#if ENABLE_FEATURE_VI_READONLY
			((vi_readonly || readonly) ? " [Read only]" : ""),
#endif
			li, ch);
		if (ch > 0) {
			// if the insert is before "dot" then we need to update
			if (q <= dot)
				dot += ch;
			file_modified++;
		}
	} else if (strncasecmp(cmd, "rewind", i) == 0) {	// rewind cmd line args
		if (file_modified && ! useforce) {
			psbs("No write since last change (:rewind! overrides)");
		} else {
			// reset the filenames to edit
			optind = fn_start - 1;
			editing = 0;
		}
#if ENABLE_FEATURE_VI_SET
	} else if (strncasecmp(cmd, "set", i) == 0) {	// set or clear features
#if ENABLE_FEATURE_VI_SETOPTS
		char *argp;
#endif
		i = 0;			// offset into args
		// only blank is regarded as args delmiter. What about tab '\t' ?
		if (!args[0] || strcasecmp(args, "all") == 0) {
			// print out values of all options
			place_cursor(rows - 1, 0, FALSE);	// go to Status line, bottom of screen
			clear_to_eol();	// clear the line
			printf("----------------------------------------\r\n");
#if ENABLE_FEATURE_VI_SETOPTS
			if (!autoindent)
				printf("no");
			printf("autoindent ");
			if (!err_method)
				printf("no");
			printf("flash ");
			if (!ignorecase)
				printf("no");
			printf("ignorecase ");
			if (!showmatch)
				printf("no");
			printf("showmatch ");
			printf("tabstop=%d ", tabstop);
#endif
			printf("\r\n");
			goto vc2;
		}
#if ENABLE_FEATURE_VI_SETOPTS
		argp = args;
		while (*argp) { 
			if (strncasecmp(argp, "no", 2) == 0)
				i = 2;		// ":set noautoindent"
			setops(argp, "autoindent ", i, "ai", VI_AUTOINDENT);
			setops(argp, "flash ", i, "fl", VI_ERR_METHOD);
			setops(argp, "ignorecase ", i, "ic", VI_IGNORECASE);
			setops(argp, "showmatch ", i, "ic", VI_SHOWMATCH);
			/* tabstopXXXX */
			if (strncasecmp(argp + i, "tabstop=%d ", 7) == 0) {
				sscanf(strchr(argp + i, '='), "tabstop=%d" + 7, &ch);
				if (ch > 0 && ch < columns - 1)
					tabstop = ch;
			}
			while (*argp && *argp != ' ')
				argp++; // skip to arg delimiter (i.e. blank)
			while (*argp && *argp == ' ')
				argp++; // skip all delimiting blanks
		}
#endif /* FEATURE_VI_SETOPTS */
#endif /* FEATURE_VI_SET */
#if ENABLE_FEATURE_VI_SEARCH
	} else if (strncasecmp(cmd, "s", 1) == 0) {	// substitute a pattern with a replacement pattern
		char *ls, *F, *R;
		int gflag;

		// F points to the "find" pattern
		// R points to the "replace" pattern
		// replace the cmd line delimiters "/" with NULLs
		gflag = 0;		// global replace flag
		c = orig_buf[1];	// what is the delimiter
		F = orig_buf + 2;	// start of "find"
		R = strchr(F, c);	// middle delimiter
		if (!R) goto colon_s_fail;
		*R++ = '\0';	// terminate "find"
		buf1 = strchr(R, c);
		if (!buf1) goto colon_s_fail;
		*buf1++ = '\0';	// terminate "replace"
		if (*buf1 == 'g') {	// :s/foo/bar/g
			buf1++;
			gflag++;	// turn on gflag
		}
		q = begin_line(q);
		if (b < 0) {	// maybe :s/foo/bar/
			q = begin_line(dot);	// start with cur line
			b = count_lines(text, q);	// cur line number
		}
		if (e < 0)
			e = b;		// maybe :.s/foo/bar/
		for (i = b; i <= e; i++) {	// so, :20,23 s \0 find \0 replace \0
			ls = q;		// orig line start
 vc4:
			buf1 = char_search(q, F, FORWARD, LIMITED);	// search cur line only for "find"
			if (buf1) {
				// we found the "find" pattern - delete it
				text_hole_delete(buf1, buf1 + strlen(F) - 1);
				// inset the "replace" patern
				string_insert(buf1, R);	// insert the string
				// check for "global"  :s/foo/bar/g
				if (gflag == 1) {
					if ((buf1 + strlen(R)) < end_line(ls)) {
						q = buf1 + strlen(R);
						goto vc4;	// don't let q move past cur line
					}
				}
			}
			q = next_line(ls);
		}
#endif /* FEATURE_VI_SEARCH */
	} else if (strncasecmp(cmd, "version", i) == 0) {  // show software version
		psb("%s", BB_VER " " BB_BT);
	} else if (strncasecmp(cmd, "write", i) == 0  // write text to file
	        || strncasecmp(cmd, "wq", i) == 0
	        || strncasecmp(cmd, "wn", i) == 0
	        || strncasecmp(cmd, "x", i) == 0
	) {
		// is there a file name to write to?
		if (args[0]) {
			fn = args;
		}
#if ENABLE_FEATURE_VI_READONLY
		if ((vi_readonly || readonly) && ! useforce) {
			psbs("\"%s\" File is read only", fn);
			goto vc3;
		}
#endif
		// how many lines in text[]?
		li = count_lines(q, r);
		ch = r - q + 1;
		// see if file exists- if not, its just a new file request
		if (useforce) {
			// if "fn" is not write-able, chmod u+w
			// sprintf(syscmd, "chmod u+w %s", fn);
			// system(syscmd);
			forced = TRUE;
		}
		l = file_write(fn, q, r);
		if (useforce && forced) {
			// chmod u-w
			// sprintf(syscmd, "chmod u-w %s", fn);
			// system(syscmd);
			forced = FALSE;
		}
		if (l < 0) {
			if (l == -1)
				psbs("Write error: %s", strerror(errno));
		} else {
			psb("\"%s\" %dL, %dC", fn, li, l);
			if (q == text && r == end - 1 && l == ch) {
				file_modified = 0;
				last_file_modified = -1;
			}
			if ((cmd[0] == 'x' || cmd[1] == 'q' || cmd[1] == 'n' ||
			     cmd[0] == 'X' || cmd[1] == 'Q' || cmd[1] == 'N')
			     && l == ch) {
				editing = 0;
			}
		}
#if ENABLE_FEATURE_VI_READONLY
 vc3:;
#endif
#if ENABLE_FEATURE_VI_YANKMARK
	} else if (strncasecmp(cmd, "yank", i) == 0) {	// yank lines
		if (b < 0) {	// no addr given- use defaults
			q = begin_line(dot);	// assume .,. for the range
			r = end_line(dot);
		}
		text_yank(q, r, YDreg);
		li = count_lines(q, r);
		psb("Yank %d lines (%d chars) into [%c]",
				li, strlen(reg[YDreg]), what_reg());
#endif
	} else {
		// cmd unknown
		ni(cmd);
	}
 vc1:
	dot = bound_dot(dot);	// make sure "dot" is valid
	return;
#if ENABLE_FEATURE_VI_SEARCH
 colon_s_fail:
	psb(":s expression missing delimiters");
#endif
}

#endif /* FEATURE_VI_COLON */

static void Hit_Return(void)
{
	char c;

	standout_start();	// start reverse video
	write1("[Hit return to continue]");
	standout_end();		// end reverse video
	while ((c = get_one_char()) != '\n' && c != '\r')	/*do nothing */
		;
	redraw(TRUE);		// force redraw all
}

//----- Synchronize the cursor to Dot --------------------------
static void sync_cursor(char * d, int *row, int *col)
{
	char *beg_cur;	// begin and end of "d" line
	char *end_scr;	// begin and end of screen
	char *tp;
	int cnt, ro, co;

	beg_cur = begin_line(d);	// first char of cur line

	end_scr = end_screen();	// last char of screen

	if (beg_cur < screenbegin) {
		// "d" is before  top line on screen
		// how many lines do we have to move
		cnt = count_lines(beg_cur, screenbegin);
 sc1:
		screenbegin = beg_cur;
		if (cnt > (rows - 1) / 2) {
			// we moved too many lines. put "dot" in middle of screen
			for (cnt = 0; cnt < (rows - 1) / 2; cnt++) {
				screenbegin = prev_line(screenbegin);
			}
		}
	} else if (beg_cur > end_scr) {
		// "d" is after bottom line on screen
		// how many lines do we have to move
		cnt = count_lines(end_scr, beg_cur);
		if (cnt > (rows - 1) / 2)
			goto sc1;	// too many lines
		for (ro = 0; ro < cnt - 1; ro++) {
			// move screen begin the same amount
			screenbegin = next_line(screenbegin);
			// now, move the end of screen
			end_scr = next_line(end_scr);
			end_scr = end_line(end_scr);
		}
	}
	// "d" is on screen- find out which row
	tp = screenbegin;
	for (ro = 0; ro < rows - 1; ro++) {	// drive "ro" to correct row
		if (tp == beg_cur)
			break;
		tp = next_line(tp);
	}

	// find out what col "d" is on
	co = 0;
	do {				// drive "co" to correct column
		if (*tp == '\n' || *tp == '\0')
			break;
		if (*tp == '\t') {
			//         7       - (co %    8  )
			co += ((tabstop - 1) - (co % tabstop));
		} else if (*tp < ' ' || *tp == 127) {
			co++;		// display as ^X, use 2 columns
		}
	} while (tp++ < d && ++co);

	// "co" is the column where "dot" is.
	// The screen has "columns" columns.
	// The currently displayed columns are  0+offset -- columns+ofset
	// |-------------------------------------------------------------|
	//               ^ ^                                ^
	//        offset | |------- columns ----------------|
	//
	// If "co" is already in this range then we do not have to adjust offset
	//      but, we do have to subtract the "offset" bias from "co".
	// If "co" is outside this range then we have to change "offset".
	// If the first char of a line is a tab the cursor will try to stay
	//  in column 7, but we have to set offset to 0.

	if (co < 0 + offset) {
		offset = co;
	}
	if (co >= columns + offset) {
		offset = co - columns + 1;
	}
	// if the first char of the line is a tab, and "dot" is sitting on it
	//  force offset to 0.
	if (d == beg_cur && *d == '\t') {
		offset = 0;
	}
	co -= offset;

	*row = ro;
	*col = co;
}

//----- Text Movement Routines ---------------------------------
static char *begin_line(char * p) // return pointer to first char cur line
{
	while (p > text && p[-1] != '\n')
		p--;			// go to cur line B-o-l
	return p;
}

static char *end_line(char * p) // return pointer to NL of cur line line
{
	while (p < end - 1 && *p != '\n')
		p++;			// go to cur line E-o-l
	return p;
}

static inline char *dollar_line(char * p) // return pointer to just before NL line
{
	while (p < end - 1 && *p != '\n')
		p++;			// go to cur line E-o-l
	// Try to stay off of the Newline
	if (*p == '\n' && (p - begin_line(p)) > 0)
		p--;
	return p;
}

static char *prev_line(char * p) // return pointer first char prev line
{
	p = begin_line(p);	// goto begining of cur line
	if (p[-1] == '\n' && p > text)
		p--;			// step to prev line
	p = begin_line(p);	// goto begining of prev line
	return p;
}

static char *next_line(char * p) // return pointer first char next line
{
	p = end_line(p);
	if (*p == '\n' && p < end - 1)
		p++;			// step to next line
	return p;
}

//----- Text Information Routines ------------------------------
static char *end_screen(void)
{
	char *q;
	int cnt;

	// find new bottom line
	q = screenbegin;
	for (cnt = 0; cnt < rows - 2; cnt++)
		q = next_line(q);
	q = end_line(q);
	return q;
}

static int count_lines(char * start, char * stop) // count line from start to stop
{
	char *q;
	int cnt;

	if (stop < start) {	// start and stop are backwards- reverse them
		q = start;
		start = stop;
		stop = q;
	}
	cnt = 0;
	stop = end_line(stop);	// get to end of this line
	for (q = start; q <= stop && q <= end - 1; q++) {
		if (*q == '\n')
			cnt++;
	}
	return cnt;
}

static char *find_line(int li)	// find begining of line #li
{
	char *q;

	for (q = text; li > 1; li--) {
		q = next_line(q);
	}
	return q;
}

//----- Dot Movement Routines ----------------------------------
static void dot_left(void)
{
	if (dot > text && dot[-1] != '\n')
		dot--;
}

static void dot_right(void)
{
	if (dot < end - 1 && *dot != '\n')
		dot++;
}

static void dot_begin(void)
{
	dot = begin_line(dot);	// return pointer to first char cur line
}

static void dot_end(void)
{
	dot = end_line(dot);	// return pointer to last char cur line
}

static char *move_to_col(char * p, int l)
{
	int co;

	p = begin_line(p);
	co = 0;
	do {
		if (*p == '\n' || *p == '\0')
			break;
		if (*p == '\t') {
			//         7       - (co %    8  )
			co += ((tabstop - 1) - (co % tabstop));
		} else if (*p < ' ' || *p == 127) {
			co++;		// display as ^X, use 2 columns
		}
	} while (++co <= l && p++ < end);
	return p;
}

static void dot_next(void)
{
	dot = next_line(dot);
}

static void dot_prev(void)
{
	dot = prev_line(dot);
}

static void dot_scroll(int cnt, int dir)
{
	char *q;

	for (; cnt > 0; cnt--) {
		if (dir < 0) {
			// scroll Backwards
			// ctrl-Y  scroll up one line
			screenbegin = prev_line(screenbegin);
		} else {
			// scroll Forwards
			// ctrl-E  scroll down one line
			screenbegin = next_line(screenbegin);
		}
	}
	// make sure "dot" stays on the screen so we dont scroll off
	if (dot < screenbegin)
		dot = screenbegin;
	q = end_screen();	// find new bottom line
	if (dot > q)
		dot = begin_line(q);	// is dot is below bottom line?
	dot_skip_over_ws();
}

static void dot_skip_over_ws(void)
{
	// skip WS
	while (isspace(*dot) && *dot != '\n' && dot < end - 1)
		dot++;
}

static void dot_delete(void)	// delete the char at 'dot'
{
	text_hole_delete(dot, dot);
}

static char *bound_dot(char * p) // make sure  text[0] <= P < "end"
{
	if (p >= end && end > text) {
		p = end - 1;
		indicate_error('1');
	}
	if (p < text) {
		p = text;
		indicate_error('2');
	}
	return p;
}

//----- Helper Utility Routines --------------------------------

//----------------------------------------------------------------
//----- Char Routines --------------------------------------------
/* Chars that are part of a word-
 *    0123456789_ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz
 * Chars that are Not part of a word (stoppers)
 *    !"#$%&'()*+,-./:;<=>?@[\]^`{|}~
 * Chars that are WhiteSpace
 *    TAB NEWLINE VT FF RETURN SPACE
 * DO NOT COUNT NEWLINE AS WHITESPACE
 */

static char *new_screen(int ro, int co)
{
	int li;

	free(screen);
	screensize = ro * co + 8;
	screen = xmalloc(screensize);
	// initialize the new screen. assume this will be a empty file.
	screen_erase();
	//   non-existent text[] lines start with a tilde (~).
	for (li = 1; li < ro - 1; li++) {
		screen[(li * co) + 0] = '~';
	}
	return screen;
}

static char *new_text(int size)
{
	if (size < 10240)
		size = 10240;	// have a minimum size for new files
	free(text);
	text = xmalloc(size + 8);
	memset(text, '\0', size);	// clear new text[]
	//text += 4;		// leave some room for "oops"
	return text;
}

#if ENABLE_FEATURE_VI_SEARCH
static int mycmp(const char * s1, const char * s2, int len)
{
	int i;

	i = strncmp(s1, s2, len);
#if ENABLE_FEATURE_VI_SETOPTS
	if (ignorecase) {
		i = strncasecmp(s1, s2, len);
	}
#endif
	return i;
}

// search for pattern starting at p
static char *char_search(char * p, const char * pat, int dir, int range)
{
#ifndef REGEX_SEARCH
	char *start, *stop;
	int len;

	len = strlen(pat);
	if (dir == FORWARD) {
		stop = end - 1;	// assume range is p - end-1
		if (range == LIMITED)
			stop = next_line(p);	// range is to next line
		for (start = p; start < stop; start++) {
			if (mycmp(start, pat, len) == 0) {
				return start;
			}
		}
	} else if (dir == BACK) {
		stop = text;	// assume range is text - p
		if (range == LIMITED)
			stop = prev_line(p);	// range is to prev line
		for (start = p - len; start >= stop; start--) {
			if (mycmp(start, pat, len) == 0) {
				return start;
			}
		}
	}
	// pattern not found
	return NULL;
#else /* REGEX_SEARCH */
	char *q;
	struct re_pattern_buffer preg;
	int i;
	int size, range;

	re_syntax_options = RE_SYNTAX_POSIX_EXTENDED;
	preg.translate = 0;
	preg.fastmap = 0;
	preg.buffer = 0;
	preg.allocated = 0;

	// assume a LIMITED forward search
	q = next_line(p);
	q = end_line(q);
	q = end - 1;
	if (dir == BACK) {
		q = prev_line(p);
		q = text;
	}
	// count the number of chars to search over, forward or backward
	size = q - p;
	if (size < 0)
		size = p - q;
	// RANGE could be negative if we are searching backwards
	range = q - p;

	q = re_compile_pattern(pat, strlen(pat), &preg);
	if (q != 0) {
		// The pattern was not compiled
		psbs("bad search pattern: \"%s\": %s", pat, q);
		i = 0;			// return p if pattern not compiled
		goto cs1;
	}

	q = p;
	if (range < 0) {
		q = p - size;
		if (q < text)
			q = text;
	}
	// search for the compiled pattern, preg, in p[]
	// range < 0-  search backward
	// range > 0-  search forward
	// 0 < start < size
	// re_search() < 0  not found or error
	// re_search() > 0  index of found pattern
	//            struct pattern    char     int    int    int     struct reg
	// re_search (*pattern_buffer,  *string, size,  start, range,  *regs)
	i = re_search(&preg, q, size, 0, range, 0);
	if (i == -1) {
		p = 0;
		i = 0;			// return NULL if pattern not found
	}
 cs1:
	if (dir == FORWARD) {
		p = p + i;
	} else {
		p = p - i;
	}
	return p;
#endif /* REGEX_SEARCH */
}
#endif /* FEATURE_VI_SEARCH */

static char *char_insert(char * p, char c) // insert the char c at 'p'
{
	if (c == 22) {		// Is this an ctrl-V?
		p = stupid_insert(p, '^');	// use ^ to indicate literal next
		p--;			// backup onto ^
		refresh(FALSE);	// show the ^
		c = get_one_char();
		*p = c;
		p++;
		file_modified++;	// has the file been modified
	} else if (c == 27) {	// Is this an ESC?
		cmd_mode = 0;
		cmdcnt = 0;
		end_cmd_q();	// stop adding to q
		last_status_cksum = 0;	// force status update
		if ((p[-1] != '\n') && (dot > text)) {
			p--;
		}
	} else if (c == erase_char || c == 8 || c == 127) { // Is this a BS
		//     123456789
		if ((p[-1] != '\n') && (dot>text)) {
			p--;
			p = text_hole_delete(p, p);	// shrink buffer 1 char
		}
	} else {
		// insert a char into text[]
		char *sp;		// "save p"

		if (c == 13)
			c = '\n';	// translate \r to \n
		sp = p;			// remember addr of insert
		p = stupid_insert(p, c);	// insert the char
#if ENABLE_FEATURE_VI_SETOPTS
		if (showmatch && strchr(")]}", *sp) != NULL) {
			showmatching(sp);
		}
		if (autoindent && c == '\n') {	// auto indent the new line
			char *q;

			q = prev_line(p);	// use prev line as templet
			for (; isblnk(*q); q++) {
				p = stupid_insert(p, *q);	// insert the char
			}
		}
#endif
	}
	return p;
}

static char *stupid_insert(char * p, char c) // stupidly insert the char c at 'p'
{
	p = text_hole_make(p, 1);
	if (p != 0) {
		*p = c;
		file_modified++;	// has the file been modified
		p++;
	}
	return p;
}

static char find_range(char ** start, char ** stop, char c)
{
	char *save_dot, *p, *q;
	int cnt;

	save_dot = dot;
	p = q = dot;

	if (strchr("cdy><", c)) {
		// these cmds operate on whole lines
		p = q = begin_line(p);
		for (cnt = 1; cnt < cmdcnt; cnt++) {
			q = next_line(q);
		}
		q = end_line(q);
	} else if (strchr("^%$0bBeEft", c)) {
		// These cmds operate on char positions
		do_cmd(c);		// execute movement cmd
		q = dot;
	} else if (strchr("wW", c)) {
		do_cmd(c);		// execute movement cmd
		// if we are at the next word's first char
		// step back one char
		// but check the possibilities when it is true
		if (dot > text && ((isspace(dot[-1]) && !isspace(dot[0]))
				|| (ispunct(dot[-1]) && !ispunct(dot[0]))
				|| (isalnum(dot[-1]) && !isalnum(dot[0]))))
			dot--;		// move back off of next word
		if (dot > text && *dot == '\n')
			dot--;		// stay off NL
		q = dot;
	} else if (strchr("H-k{", c)) {
		// these operate on multi-lines backwards
		q = end_line(dot);	// find NL
		do_cmd(c);		// execute movement cmd
		dot_begin();
		p = dot;
	} else if (strchr("L+j}\r\n", c)) {
		// these operate on multi-lines forwards
		p = begin_line(dot);
		do_cmd(c);		// execute movement cmd
		dot_end();		// find NL
		q = dot;
	} else {
		c = 27;			// error- return an ESC char
		//break;
	}
	*start = p;
	*stop = q;
	if (q < p) {
		*start = q;
		*stop = p;
	}
	dot = save_dot;
	return c;
}

static int st_test(char * p, int type, int dir, char * tested)
{
	char c, c0, ci;
	int test, inc;

	inc = dir;
	c = c0 = p[0];
	ci = p[inc];
	test = 0;

	if (type == S_BEFORE_WS) {
		c = ci;
		test = ((!isspace(c)) || c == '\n');
	}
	if (type == S_TO_WS) {
		c = c0;
		test = ((!isspace(c)) || c == '\n');
	}
	if (type == S_OVER_WS) {
		c = c0;
		test = ((isspace(c)));
	}
	if (type == S_END_PUNCT) {
		c = ci;
		test = ((ispunct(c)));
	}
	if (type == S_END_ALNUM) {
		c = ci;
		test = ((isalnum(c)) || c == '_');
	}
	*tested = c;
	return test;
}

static char *skip_thing(char * p, int linecnt, int dir, int type)
{
	char c;

	while (st_test(p, type, dir, &c)) {
		// make sure we limit search to correct number of lines
		if (c == '\n' && --linecnt < 1)
			break;
		if (dir >= 0 && p >= end - 1)
			break;
		if (dir < 0 && p <= text)
			break;
		p += dir;		// move to next char
	}
	return p;
}

// find matching char of pair  ()  []  {}
static char *find_pair(char * p, char c)
{
	char match, *q;
	int dir, level;

	match = ')';
	level = 1;
	dir = 1;			// assume forward
	switch (c) {
	case '(':
		match = ')';
		break;
	case '[':
		match = ']';
		break;
	case '{':
		match = '}';
		break;
	case ')':
		match = '(';
		dir = -1;
		break;
	case ']':
		match = '[';
		dir = -1;
		break;
	case '}':
		match = '{';
		dir = -1;
		break;
	}
	for (q = p + dir; text <= q && q < end; q += dir) {
		// look for match, count levels of pairs  (( ))
		if (*q == c)
			level++;	// increase pair levels
		if (*q == match)
			level--;	// reduce pair level
		if (level == 0)
			break;		// found matching pair
	}
	if (level != 0)
		q = NULL;		// indicate no match
	return q;
}

#if ENABLE_FEATURE_VI_SETOPTS
// show the matching char of a pair,  ()  []  {}
static void showmatching(char * p)
{
	char *q, *save_dot;

	// we found half of a pair
	q = find_pair(p, *p);	// get loc of matching char
	if (q == NULL) {
		indicate_error('3');	// no matching char
	} else {
		// "q" now points to matching pair
		save_dot = dot;	// remember where we are
		dot = q;		// go to new loc
		refresh(FALSE);	// let the user see it
		mysleep(40);	// give user some time
		dot = save_dot;	// go back to old loc
		refresh(FALSE);
	}
}
#endif /* FEATURE_VI_SETOPTS */

//  open a hole in text[]
static char *text_hole_make(char * p, int size)	// at "p", make a 'size' byte hole
{
	char *src, *dest;
	int cnt;

	if (size <= 0)
		goto thm0;
	src = p;
	dest = p + size;
	cnt = end - src;	// the rest of buffer
	if (memmove(dest, src, cnt) != dest) {
		psbs("can't create room for new characters");
	}
	memset(p, ' ', size);	// clear new hole
	end = end + size;	// adjust the new END
	file_modified++;	// has the file been modified
 thm0:
	return p;
}

//  close a hole in text[]
static char *text_hole_delete(char * p, char * q) // delete "p" thru "q", inclusive
{
	char *src, *dest;
	int cnt, hole_size;

	// move forwards, from beginning
	// assume p <= q
	src = q + 1;
	dest = p;
	if (q < p) {		// they are backward- swap them
		src = p + 1;
		dest = q;
	}
	hole_size = q - p + 1;
	cnt = end - src;
	if (src < text || src > end)
		goto thd0;
	if (dest < text || dest >= end)
		goto thd0;
	if (src >= end)
		goto thd_atend;	// just delete the end of the buffer
	if (memmove(dest, src, cnt) != dest) {
		psbs("can't delete the character");
	}
 thd_atend:
	end = end - hole_size;	// adjust the new END
	if (dest >= end)
		dest = end - 1;	// make sure dest in below end-1
	if (end <= text)
		dest = end = text;	// keep pointers valid
	file_modified++;	// has the file been modified
 thd0:
	return dest;
}

// copy text into register, then delete text.
// if dist <= 0, do not include, or go past, a NewLine
//
static char *yank_delete(char * start, char * stop, int dist, int yf)
{
	char *p;

	// make sure start <= stop
	if (start > stop) {
		// they are backwards, reverse them
		p = start;
		start = stop;
		stop = p;
	}
	if (dist <= 0) {
		// we cannot cross NL boundaries
		p = start;
		if (*p == '\n')
			return p;
		// dont go past a NewLine
		for (; p + 1 <= stop; p++) {
			if (p[1] == '\n') {
				stop = p;	// "stop" just before NewLine
				break;
			}
		}
	}
	p = start;
#if ENABLE_FEATURE_VI_YANKMARK
	text_yank(start, stop, YDreg);
#endif
	if (yf == YANKDEL) {
		p = text_hole_delete(start, stop);
	}					// delete lines
	return p;
}

static void show_help(void)
{
	puts("These features are available:"
#if ENABLE_FEATURE_VI_SEARCH
	"\n\tPattern searches with / and ?"
#endif
#if ENABLE_FEATURE_VI_DOT_CMD
	"\n\tLast command repeat with \'.\'"
#endif
#if ENABLE_FEATURE_VI_YANKMARK
	"\n\tLine marking with  'x"
	"\n\tNamed buffers with  \"x"
#endif
#if ENABLE_FEATURE_VI_READONLY
	"\n\tReadonly if vi is called as \"view\""
	"\n\tReadonly with -R command line arg"
#endif
#if ENABLE_FEATURE_VI_SET
	"\n\tSome colon mode commands with \':\'"
#endif
#if ENABLE_FEATURE_VI_SETOPTS
	"\n\tSettable options with \":set\""
#endif
#if ENABLE_FEATURE_VI_USE_SIGNALS
	"\n\tSignal catching- ^C"
	"\n\tJob suspend and resume with ^Z"
#endif
#if ENABLE_FEATURE_VI_WIN_RESIZE
	"\n\tAdapt to window re-sizes"
#endif
	);
}

static inline void print_literal(char * buf, const char * s) // copy s to buf, convert unprintable
{
	unsigned char c;
	char b[2];

	b[1] = '\0';
	buf[0] = '\0';
	if (!s[0])
		s = "(NULL)";
	for (; *s; s++) {
		int c_is_no_print;

		c = *s;
		c_is_no_print = (c & 0x80) && !Isprint(c);
		if (c_is_no_print) {
			strcat(buf, SOn);
			c = '.';
		}
		if (c < ' ' || c == 127) {
			strcat(buf, "^");
			if (c == 127)
				c = '?';
			else
				c += '@';
		}
		b[0] = c;
		strcat(buf, b);
		if (c_is_no_print)
			strcat(buf, SOs);
		if (*s == '\n')
			strcat(buf, "$");
	}
}

#if ENABLE_FEATURE_VI_DOT_CMD
static void start_new_cmd_q(char c)
{
	// release old cmd
	free(last_modifying_cmd);
	// get buffer for new cmd
	last_modifying_cmd = xmalloc(BUFSIZ);
	memset(last_modifying_cmd, '\0', BUFSIZ);	// clear new cmd queue
	// if there is a current cmd count put it in the buffer first
	if (cmdcnt > 0)
		sprintf(last_modifying_cmd, "%d%c", cmdcnt, c);
	else // just save char c onto queue
		last_modifying_cmd[0] = c;
	adding2q = 1;
}

static void end_cmd_q(void)
{
#if ENABLE_FEATURE_VI_YANKMARK
	YDreg = 26;			// go back to default Yank/Delete reg
#endif
	adding2q = 0;
}
#endif /* FEATURE_VI_DOT_CMD */

#if ENABLE_FEATURE_VI_YANKMARK \
 || (ENABLE_FEATURE_VI_COLON && ENABLE_FEATURE_VI_SEARCH) \
 || ENABLE_FEATURE_VI_CRASHME
static char *string_insert(char * p, char * s) // insert the string at 'p'
{
	int cnt, i;

	i = strlen(s);
	p = text_hole_make(p, i);
	strncpy(p, s, i);
	for (cnt = 0; *s != '\0'; s++) {
		if (*s == '\n')
			cnt++;
	}
#if ENABLE_FEATURE_VI_YANKMARK
	psb("Put %d lines (%d chars) from [%c]", cnt, i, what_reg());
#endif
	return p;
}
#endif

#if ENABLE_FEATURE_VI_YANKMARK
static char *text_yank(char * p, char * q, int dest)	// copy text into a register
{
	char *t;
	int cnt;

	if (q < p) {		// they are backwards- reverse them
		t = q;
		q = p;
		p = t;
	}
	cnt = q - p + 1;
	t = reg[dest];
	free(t);		//  if already a yank register, free it
	t = xmalloc(cnt + 1);	// get a new register
	memset(t, '\0', cnt + 1);	// clear new text[]
	strncpy(t, p, cnt);	// copy text[] into bufer
	reg[dest] = t;
	return p;
}

static char what_reg(void)
{
	char c;

	c = 'D';			// default to D-reg
	if (0 <= YDreg && YDreg <= 25)
		c = 'a' + (char) YDreg;
	if (YDreg == 26)
		c = 'D';
	if (YDreg == 27)
		c = 'U';
	return c;
}

static void check_context(char cmd)
{
	// A context is defined to be "modifying text"
	// Any modifying command establishes a new context.

	if (dot < context_start || dot > context_end) {
		if (strchr(modifying_cmds, cmd) != NULL) {
			// we are trying to modify text[]- make this the current context
			mark[27] = mark[26];	// move cur to prev
			mark[26] = dot;	// move local to cur
			context_start = prev_line(prev_line(dot));
			context_end = next_line(next_line(dot));
			//loiter= start_loiter= now;
		}
	}
}

static inline char *swap_context(char * p) // goto new context for '' command make this the current context
{
	char *tmp;

	// the current context is in mark[26]
	// the previous context is in mark[27]
	// only swap context if other context is valid
	if (text <= mark[27] && mark[27] <= end - 1) {
		tmp = mark[27];
		mark[27] = mark[26];
		mark[26] = tmp;
		p = mark[26];	// where we are going- previous context
		context_start = prev_line(prev_line(prev_line(p)));
		context_end = next_line(next_line(next_line(p)));
	}
	return p;
}
#endif /* FEATURE_VI_YANKMARK */

static int isblnk(char c) // is the char a blank or tab
{
	return (c == ' ' || c == '\t');
}

//----- Set terminal attributes --------------------------------
static void rawmode(void)
{
	tcgetattr(0, &term_orig);
	term_vi = term_orig;
	term_vi.c_lflag &= (~ICANON & ~ECHO);	// leave ISIG ON- allow intr's
	term_vi.c_iflag &= (~IXON & ~ICRNL);
	term_vi.c_oflag &= (~ONLCR);
	term_vi.c_cc[VMIN] = 1;
	term_vi.c_cc[VTIME] = 0;
	erase_char = term_vi.c_cc[VERASE];
	tcsetattr(0, TCSANOW, &term_vi);
}

static void cookmode(void)
{
	fflush(stdout);
	tcsetattr(0, TCSANOW, &term_orig);
}

//----- Come here when we get a window resize signal ---------
#if ENABLE_FEATURE_VI_USE_SIGNALS
static void winch_sig(int sig ATTRIBUTE_UNUSED)
{
	signal(SIGWINCH, winch_sig);
	if (ENABLE_FEATURE_VI_WIN_RESIZE)
		get_terminal_width_height(0, &columns, &rows);
	new_screen(rows, columns);	// get memory for virtual screen
	redraw(TRUE);		// re-draw the screen
}

//----- Come here when we get a continue signal -------------------
static void cont_sig(int sig ATTRIBUTE_UNUSED)
{
	rawmode();			// terminal to "raw"
	last_status_cksum = 0;	// force status update
	redraw(TRUE);		// re-draw the screen

	signal(SIGTSTP, suspend_sig);
	signal(SIGCONT, SIG_DFL);
	kill(my_pid, SIGCONT);
}

//----- Come here when we get a Suspend signal -------------------
static void suspend_sig(int sig ATTRIBUTE_UNUSED)
{
	place_cursor(rows - 1, 0, FALSE);	// go to bottom of screen
	clear_to_eol();		// Erase to end of line
	cookmode();			// terminal to "cooked"

	signal(SIGCONT, cont_sig);
	signal(SIGTSTP, SIG_DFL);
	kill(my_pid, SIGTSTP);
}

//----- Come here when we get a signal ---------------------------
static void catch_sig(int sig)
{
	signal(SIGINT, catch_sig);
	if (sig)
		longjmp(restart, sig);
}
#endif /* FEATURE_VI_USE_SIGNALS */

static int mysleep(int hund)	// sleep for 'h' 1/100 seconds
{
	// Don't hang- Wait 5/100 seconds-  1 Sec= 1000000
	fflush(stdout);
	FD_ZERO(&rfds);
	FD_SET(0, &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = hund * 10000;
	select(1, &rfds, NULL, NULL, &tv);
	return FD_ISSET(0, &rfds);
}

#define readbuffer bb_common_bufsiz1

static int readed_for_parse;

//----- IO Routines --------------------------------------------
static char readit(void)	// read (maybe cursor) key from stdin
{
	char c;
	int n;
	struct esc_cmds {
		const char *seq;
		char val;
	};

	static const struct esc_cmds esccmds[] = {
		{"OA", VI_K_UP},       // cursor key Up
		{"OB", VI_K_DOWN},     // cursor key Down
		{"OC", VI_K_RIGHT},    // Cursor Key Right
		{"OD", VI_K_LEFT},     // cursor key Left
		{"OH", VI_K_HOME},     // Cursor Key Home
		{"OF", VI_K_END},      // Cursor Key End
		{"[A", VI_K_UP},       // cursor key Up
		{"[B", VI_K_DOWN},     // cursor key Down
		{"[C", VI_K_RIGHT},    // Cursor Key Right
		{"[D", VI_K_LEFT},     // cursor key Left
		{"[H", VI_K_HOME},     // Cursor Key Home
		{"[F", VI_K_END},      // Cursor Key End
		{"[1~", VI_K_HOME},    // Cursor Key Home
		{"[2~", VI_K_INSERT},  // Cursor Key Insert
		{"[4~", VI_K_END},     // Cursor Key End
		{"[5~", VI_K_PAGEUP},  // Cursor Key Page Up
		{"[6~", VI_K_PAGEDOWN},// Cursor Key Page Down
		{"OP", VI_K_FUN1},     // Function Key F1
		{"OQ", VI_K_FUN2},     // Function Key F2
		{"OR", VI_K_FUN3},     // Function Key F3
		{"OS", VI_K_FUN4},     // Function Key F4
		{"[15~", VI_K_FUN5},   // Function Key F5
		{"[17~", VI_K_FUN6},   // Function Key F6
		{"[18~", VI_K_FUN7},   // Function Key F7
		{"[19~", VI_K_FUN8},   // Function Key F8
		{"[20~", VI_K_FUN9},   // Function Key F9
		{"[21~", VI_K_FUN10},  // Function Key F10
		{"[23~", VI_K_FUN11},  // Function Key F11
		{"[24~", VI_K_FUN12},  // Function Key F12
		{"[11~", VI_K_FUN1},   // Function Key F1
		{"[12~", VI_K_FUN2},   // Function Key F2
		{"[13~", VI_K_FUN3},   // Function Key F3
		{"[14~", VI_K_FUN4},   // Function Key F4
	};

#define ESCCMDS_COUNT (sizeof(esccmds)/sizeof(struct esc_cmds))

	alarm(0);	// turn alarm OFF while we wait for input
	fflush(stdout);
	n = readed_for_parse;
	// get input from User- are there already input chars in Q?
	if (n <= 0) {
 ri0:
		// the Q is empty, wait for a typed char
		n = read(0, readbuffer, BUFSIZ - 1);
		if (n < 0) {
			if (errno == EINTR)
				goto ri0;	// interrupted sys call
			if (errno == EBADF)
				editing = 0;
			if (errno == EFAULT)
				editing = 0;
			if (errno == EINVAL)
				editing = 0;
			if (errno == EIO)
				editing = 0;
			errno = 0;
		}
		if (n <= 0)
			return 0;       // error
		if (readbuffer[0] == 27) {
			// This is an ESC char. Is this Esc sequence?
			// Could be bare Esc key. See if there are any
			// more chars to read after the ESC. This would
			// be a Function or Cursor Key sequence.
			FD_ZERO(&rfds);
			FD_SET(0, &rfds);
			tv.tv_sec = 0;
			tv.tv_usec = 50000;	// Wait 5/100 seconds- 1 Sec=1000000

			// keep reading while there are input chars and room in buffer
			while (select(1, &rfds, NULL, NULL, &tv) > 0 && n <= (BUFSIZ - 5)) {
				// read the rest of the ESC string
				int r = read(0, (void *) (readbuffer + n), BUFSIZ - n);
				if (r > 0) {
					n += r;
				}
			}
		}
		readed_for_parse = n;
	}
	c = readbuffer[0];
	if (c == 27 && n > 1) {
		// Maybe cursor or function key?
		const struct esc_cmds *eindex;

		for (eindex = esccmds; eindex < &esccmds[ESCCMDS_COUNT]; eindex++) {
			int cnt = strlen(eindex->seq);

			if (n <= cnt)
				continue;
			if (strncmp(eindex->seq, readbuffer + 1, cnt))
				continue;
			// is a Cursor key- put derived value back into Q
			c = eindex->val;
			// for squeeze out the ESC sequence
			n = cnt + 1;
			break;
		}
		if (eindex == &esccmds[ESCCMDS_COUNT]) {
			/* defined ESC sequence not found, set only one ESC */
			n = 1;
	}
	} else {
		n = 1;
	}
	// remove key sequence from Q
	readed_for_parse -= n;
	memmove(readbuffer, readbuffer + n, BUFSIZ - n);
	alarm(3);	// we are done waiting for input, turn alarm ON
	return c;
}

//----- IO Routines --------------------------------------------
static char get_one_char(void)
{
	static char c;

#if ENABLE_FEATURE_VI_DOT_CMD
	// ! adding2q  && ioq == 0  read()
	// ! adding2q  && ioq != 0  *ioq
	// adding2q         *last_modifying_cmd= read()
	if (!adding2q) {
		// we are not adding to the q.
		// but, we may be reading from a q
		if (ioq == 0) {
			// there is no current q, read from STDIN
			c = readit();	// get the users input
		} else {
			// there is a queue to get chars from first
			c = *ioq++;
			if (c == '\0') {
				// the end of the q, read from STDIN
				free(ioq_start);
				ioq_start = ioq = 0;
				c = readit();	// get the users input
			}
		}
	} else {
		// adding STDIN chars to q
		c = readit();	// get the users input
		if (last_modifying_cmd != 0) {
			int len = strlen(last_modifying_cmd);
			if (len + 1 >= BUFSIZ) {
				psbs("last_modifying_cmd overrun");
			} else {
				// add new char to q
				last_modifying_cmd[len] = c;
			}
		}
	}
#else
	c = readit();		// get the users input
#endif /* FEATURE_VI_DOT_CMD */
	return c;			// return the char, where ever it came from
}

static char *get_input_line(const char * prompt) // get input line- use "status line"
{
	static char *obufp;

	char buf[BUFSIZ];
	char c;
	int i;

	strcpy(buf, prompt);
	last_status_cksum = 0;	// force status update
	place_cursor(rows - 1, 0, FALSE);	// go to Status line, bottom of screen
	clear_to_eol();		// clear the line
	write1(prompt);      // write out the :, /, or ? prompt

	i = strlen(buf);
	while (i < BUFSIZ) {
		c = get_one_char();	// read user input
		if (c == '\n' || c == '\r' || c == 27)
			break;		// is this end of input
		if (c == erase_char || c == 8 || c == 127) {
			// user wants to erase prev char
			i--;		// backup to prev char
			buf[i] = '\0';	// erase the char
			buf[i + 1] = '\0';	// null terminate buffer
			write1("\b \b");     // erase char on screen
			if (i <= 0) {	// user backs up before b-o-l, exit
				break;
			}
		} else {
			buf[i] = c;	// save char in buffer
			buf[i + 1] = '\0';	// make sure buffer is null terminated
			putchar(c);   // echo the char back to user
			i++;
		}
	}
	refresh(FALSE);
	free(obufp);
	obufp = xstrdup(buf);
	return obufp;
}

static int file_size(const char * fn) // what is the byte size of "fn"
{
	struct stat st_buf;
	int cnt, sr;

	if (!fn || !fn[0])
		return -1;
	cnt = -1;
	sr = stat(fn, &st_buf);	// see if file exists
	if (sr >= 0) {
		cnt = (int) st_buf.st_size;
	}
	return cnt;
}

static int file_insert(char * fn, char * p, int size)
{
	int fd, cnt;

	cnt = -1;
#if ENABLE_FEATURE_VI_READONLY
	readonly = FALSE;
#endif
	if (!fn || !fn[0]) {
		psbs("No filename given");
		goto fi0;
	}
	if (size == 0) {
		// OK- this is just a no-op
		cnt = 0;
		goto fi0;
	}
	if (size < 0) {
		psbs("Trying to insert a negative number (%d) of characters", size);
		goto fi0;
	}
	if (p < text || p > end) {
		psbs("Trying to insert file outside of memory");
		goto fi0;
	}

	// see if we can open the file
#if ENABLE_FEATURE_VI_READONLY
	if (vi_readonly) goto fi1;		// do not try write-mode
#endif
	fd = open(fn, O_RDWR);			// assume read & write
	if (fd < 0) {
		// could not open for writing- maybe file is read only
#if ENABLE_FEATURE_VI_READONLY
 fi1:
#endif
		fd = open(fn, O_RDONLY);	// try read-only
		if (fd < 0) {
			psbs("\"%s\" %s", fn, "cannot open file");
			goto fi0;
		}
#if ENABLE_FEATURE_VI_READONLY
		// got the file- read-only
		readonly = TRUE;
#endif
	}
	p = text_hole_make(p, size);
	cnt = read(fd, p, size);
	close(fd);
	if (cnt < 0) {
		cnt = -1;
		p = text_hole_delete(p, p + size - 1);	// un-do buffer insert
		psbs("cannot read file \"%s\"", fn);
	} else if (cnt < size) {
		// There was a partial read, shrink unused space text[]
		p = text_hole_delete(p + cnt, p + (size - cnt) - 1);	// un-do buffer insert
		psbs("cannot read all of file \"%s\"", fn);
	}
	if (cnt >= size)
		file_modified++;
 fi0:
	return cnt;
}

static int file_write(char * fn, char * first, char * last)
{
	int fd, cnt, charcnt;

	if (fn == 0) {
		psbs("No current filename");
		return -2;
	}
	charcnt = 0;
	// FIXIT- use the correct umask()
	fd = open(fn, (O_WRONLY | O_CREAT | O_TRUNC), 0664);
	if (fd < 0)
		return -1;
	cnt = last - first + 1;
	charcnt = write(fd, first, cnt);
	if (charcnt == cnt) {
		// good write
		//file_modified = FALSE; // the file has not been modified
	} else {
		charcnt = 0;
	}
	close(fd);
	return charcnt;
}

//----- Terminal Drawing ---------------------------------------
// The terminal is made up of 'rows' line of 'columns' columns.
// classically this would be 24 x 80.
//  screen coordinates
//  0,0     ...     0,79
//  1,0     ...     1,79
//  .       ...     .
//  .       ...     .
//  22,0    ...     22,79
//  23,0    ...     23,79   status line
//

//----- Move the cursor to row x col (count from 0, not 1) -------
static void place_cursor(int row, int col, int opti)
{
	char cm1[BUFSIZ];
	char *cm;
#if ENABLE_FEATURE_VI_OPTIMIZE_CURSOR
	char cm2[BUFSIZ];
	char *screenp;
	// char cm3[BUFSIZ];
	int Rrow = last_row;
#endif

	memset(cm1, '\0', BUFSIZ - 1);  // clear the buffer

	if (row < 0) row = 0;
	if (row >= rows) row = rows - 1;
	if (col < 0) col = 0;
	if (col >= columns) col = columns - 1;

	//----- 1.  Try the standard terminal ESC sequence
	sprintf(cm1, CMrc, row + 1, col + 1);
	cm = cm1;
	if (!opti)
		goto pc0;

#if ENABLE_FEATURE_VI_OPTIMIZE_CURSOR
	//----- find the minimum # of chars to move cursor -------------
	//----- 2.  Try moving with discreet chars (Newline, [back]space, ...)
	memset(cm2, '\0', BUFSIZ - 1);  // clear the buffer

	// move to the correct row
	while (row < Rrow) {
		// the cursor has to move up
		strcat(cm2, CMup);
		Rrow--;
	}
	while (row > Rrow) {
		// the cursor has to move down
		strcat(cm2, CMdown);
		Rrow++;
	}

	// now move to the correct column
	strcat(cm2, "\r");			// start at col 0
	// just send out orignal source char to get to correct place
	screenp = &screen[row * columns];	// start of screen line
	strncat(cm2, screenp, col);

	//----- 3.  Try some other way of moving cursor
	//---------------------------------------------

	// pick the shortest cursor motion to send out
	cm = cm1;
	if (strlen(cm2) < strlen(cm)) {
		cm = cm2;
	}  /* else if (strlen(cm3) < strlen(cm)) {
		cm= cm3;
	} */
#endif /* FEATURE_VI_OPTIMIZE_CURSOR */
 pc0:
	write1(cm);                 // move the cursor
}

//----- Erase from cursor to end of line -----------------------
static void clear_to_eol(void)
{
	write1(Ceol);   // Erase from cursor to end of line
}

//----- Erase from cursor to end of screen -----------------------
static void clear_to_eos(void)
{
	write1(Ceos);   // Erase from cursor to end of screen
}

//----- Start standout mode ------------------------------------
static void standout_start(void) // send "start reverse video" sequence
{
	write1(SOs);     // Start reverse video mode
}

//----- End standout mode --------------------------------------
static void standout_end(void) // send "end reverse video" sequence
{
	write1(SOn);     // End reverse video mode
}

//----- Flash the screen  --------------------------------------
static void flash(int h)
{
	standout_start();	// send "start reverse video" sequence
	redraw(TRUE);
	mysleep(h);
	standout_end();		// send "end reverse video" sequence
	redraw(TRUE);
}

static void Indicate_Error(void)
{
#if ENABLE_FEATURE_VI_CRASHME
	if (crashme > 0)
		return;			// generate a random command
#endif
	if (!err_method) {
		write1(bell);   // send out a bell character
	} else {
		flash(10);
	}
}

//----- Screen[] Routines --------------------------------------
//----- Erase the Screen[] memory ------------------------------
static void screen_erase(void)
{
	memset(screen, ' ', screensize);	// clear new screen
}

static int bufsum(char *buf, int count)
{
	int sum = 0;
	char *e = buf + count;

	while (buf < e)
		sum += (unsigned char) *buf++;
	return sum;
}

//----- Draw the status line at bottom of the screen -------------
static void show_status_line(void)
{
	int cnt = 0, cksum = 0;

	// either we already have an error or status message, or we
	// create one.
	if (!have_status_msg) {
		cnt = format_edit_status();
		cksum = bufsum(status_buffer, cnt);
	}
	if (have_status_msg || ((cnt > 0 && last_status_cksum != cksum))) {
		last_status_cksum= cksum;		// remember if we have seen this line
		place_cursor(rows - 1, 0, FALSE);	// put cursor on status line
		write1(status_buffer);
		clear_to_eol();
		if (have_status_msg) {
			if (((int)strlen(status_buffer) - (have_status_msg - 1)) >
					(columns - 1) ) {
				have_status_msg = 0;
				Hit_Return();
			}
			have_status_msg = 0;
		}
		place_cursor(crow, ccol, FALSE);	// put cursor back in correct place
	}
	fflush(stdout);
}

//----- format the status buffer, the bottom line of screen ------
// format status buffer, with STANDOUT mode
static void psbs(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	strcpy(status_buffer, SOs);	// Terminal standout mode on
	vsprintf(status_buffer + strlen(status_buffer), format, args);
	strcat(status_buffer, SOn);	// Terminal standout mode off
	va_end(args);

	have_status_msg = 1 + sizeof(SOs) + sizeof(SOn) - 2;
}

// format status buffer
static void psb(const char *format, ...)
{
	va_list args;

	va_start(args, format);
	vsprintf(status_buffer, format, args);
	va_end(args);

	have_status_msg = 1;
}

static void ni(const char * s) // display messages
{
	char buf[BUFSIZ];

	print_literal(buf, s);
	psbs("\'%s\' is not implemented", buf);
}

static int format_edit_status(void)	// show file status on status line
{
	static int tot;

	int cur, percent, ret, trunc_at;

	// file_modified is now a counter rather than a flag.  this
	// helps reduce the amount of line counting we need to do.
	// (this will cause a mis-reporting of modified status
	// once every MAXINT editing operations.)

	// it would be nice to do a similar optimization here -- if
	// we haven't done a motion that could have changed which line
	// we're on, then we shouldn't have to do this count_lines()
	cur = count_lines(text, dot);

	// reduce counting -- the total lines can't have
	// changed if we haven't done any edits.
	if (file_modified != last_file_modified) {
		tot = cur + count_lines(dot, end - 1) - 1;
		last_file_modified = file_modified;
	}

	//    current line         percent
	//   -------------    ~~ ----------
	//    total lines            100
	if (tot > 0) {
		percent = (100 * cur) / tot;
	} else {
		cur = tot = 0;
		percent = 100;
	}

	trunc_at = columns < STATUS_BUFFER_LEN-1 ?
		columns : STATUS_BUFFER_LEN-1;

	ret = snprintf(status_buffer, trunc_at+1,
#if ENABLE_FEATURE_VI_READONLY
		"%c %s%s%s %d/%d %d%%",
#else
		"%c %s%s %d/%d %d%%",
#endif
		(cmd_mode ? (cmd_mode == 2 ? 'R':'I'):'-'),
		(cfn != 0 ? cfn : "No file"),
#if ENABLE_FEATURE_VI_READONLY
		((vi_readonly || readonly) ? " [Read-only]" : ""),
#endif
		(file_modified ? " [modified]" : ""),
		cur, tot, percent);

	if (ret >= 0 && ret < trunc_at)
		return ret;  /* it all fit */

	return trunc_at;  /* had to truncate */
}

//----- Force refresh of all Lines -----------------------------
static void redraw(int full_screen)
{
	place_cursor(0, 0, FALSE);	// put cursor in correct place
	clear_to_eos();		// tel terminal to erase display
	screen_erase();		// erase the internal screen buffer
	last_status_cksum = 0;	// force status update
	refresh(full_screen);	// this will redraw the entire display
	show_status_line();
}

//----- Format a text[] line into a buffer ---------------------
static void format_line(char *dest, char *src, int li)
{
	int co;
	char c;

	for (co = 0; co < MAX_SCR_COLS; co++) {
		c = ' ';		// assume blank
		if (li > 0 && co == 0) {
			c = '~';        // not first line, assume Tilde
		}
		// are there chars in text[] and have we gone past the end
		if (text < end && src < end) {
			c = *src++;
		}
		if (c == '\n')
			break;
		if ((c & 0x80) && !Isprint(c)) {
			c = '.';
		}
		if ((unsigned char)(c) < ' ' || c == 0x7f) {
			if (c == '\t') {
				c = ' ';
				//       co %    8     !=     7
				for (; (co % tabstop) != (tabstop - 1); co++) {
					dest[co] = c;
				}
			} else {
				dest[co++] = '^';
				if (c == 0x7f)
					c = '?';
				else
					c += '@';       // make it visible
			}
		}
		// the co++ is done here so that the column will
		// not be overwritten when we blank-out the rest of line
		dest[co] = c;
		if (src >= end)
			break;
	}
}

//----- Refresh the changed screen lines -----------------------
// Copy the source line from text[] into the buffer and note
// if the current screenline is different from the new buffer.
// If they differ then that line needs redrawing on the terminal.
//
static void refresh(int full_screen)
{
	static int old_offset;

	int li, changed;
	char buf[MAX_SCR_COLS];
	char *tp, *sp;		// pointer into text[] and screen[]
#if ENABLE_FEATURE_VI_OPTIMIZE_CURSOR
	int last_li = -2; // last line that changed- for optimizing cursor movement
#endif

	if (ENABLE_FEATURE_VI_WIN_RESIZE)
		get_terminal_width_height(0, &columns, &rows);
	sync_cursor(dot, &crow, &ccol);	// where cursor will be (on "dot")
	tp = screenbegin;	// index into text[] of top line

	// compare text[] to screen[] and mark screen[] lines that need updating
	for (li = 0; li < rows - 1; li++) {
		int cs, ce;				// column start & end
		memset(buf, ' ', MAX_SCR_COLS);		// blank-out the buffer
		buf[MAX_SCR_COLS-1] = 0;		// NULL terminate the buffer
		// format current text line into buf
		format_line(buf, tp, li);

		// skip to the end of the current text[] line
		while (tp < end && *tp++ != '\n') /*no-op*/ ;

		// see if there are any changes between vitual screen and buf
		changed = FALSE;	// assume no change
		cs= 0;
		ce= columns-1;
		sp = &screen[li * columns];	// start of screen line
		if (full_screen) {
			// force re-draw of every single column from 0 - columns-1
			goto re0;
		}
		// compare newly formatted buffer with virtual screen
		// look forward for first difference between buf and screen
		for (; cs <= ce; cs++) {
			if (buf[cs + offset] != sp[cs]) {
				changed = TRUE;	// mark for redraw
				break;
			}
		}

		// look backward for last difference between buf and screen
		for ( ; ce >= cs; ce--) {
			if (buf[ce + offset] != sp[ce]) {
				changed = TRUE;	// mark for redraw
				break;
			}
		}
		// now, cs is index of first diff, and ce is index of last diff

		// if horz offset has changed, force a redraw
		if (offset != old_offset) {
 re0:
			changed = TRUE;
		}

		// make a sanity check of columns indexes
		if (cs < 0) cs= 0;
		if (ce > columns-1) ce= columns-1;
		if (cs > ce) {  cs= 0;  ce= columns-1;  }
		// is there a change between vitual screen and buf
		if (changed) {
			//  copy changed part of buffer to virtual screen
			memmove(sp+cs, buf+(cs+offset), ce-cs+1);

			// move cursor to column of first change
			if (offset != old_offset) {
				// opti_cur_move is still too stupid
				// to handle offsets correctly
				place_cursor(li, cs, FALSE);
			} else {
#if ENABLE_FEATURE_VI_OPTIMIZE_CURSOR
				// if this just the next line
				//  try to optimize cursor movement
				//  otherwise, use standard ESC sequence
				place_cursor(li, cs, li == (last_li+1) ? TRUE : FALSE);
				last_li= li;
#else
				place_cursor(li, cs, FALSE);	// use standard ESC sequence
#endif /* FEATURE_VI_OPTIMIZE_CURSOR */
			}

			// write line out to terminal
			{
				int nic = ce - cs + 1;
				char *out = sp + cs;

				while (nic-- > 0) {
					putchar(*out);
					out++;
				}
			}
#if ENABLE_FEATURE_VI_OPTIMIZE_CURSOR
			last_row = li;
#endif
		}
	}

#if ENABLE_FEATURE_VI_OPTIMIZE_CURSOR
	place_cursor(crow, ccol, (crow == last_row) ? TRUE : FALSE);
	last_row = crow;
#else
	place_cursor(crow, ccol, FALSE);
#endif

	if (offset != old_offset)
		old_offset = offset;
}

//---------------------------------------------------------------------
//----- the Ascii Chart -----------------------------------------------
//
//  00 nul   01 soh   02 stx   03 etx   04 eot   05 enq   06 ack   07 bel
//  08 bs    09 ht    0a nl    0b vt    0c np    0d cr    0e so    0f si
//  10 dle   11 dc1   12 dc2   13 dc3   14 dc4   15 nak   16 syn   17 etb
//  18 can   19 em    1a sub   1b esc   1c fs    1d gs    1e rs    1f us
//  20 sp    21 !     22 "     23 #     24 $     25 %     26 &     27 '
//  28 (     29 )     2a *     2b +     2c ,     2d -     2e .     2f /
//  30 0     31 1     32 2     33 3     34 4     35 5     36 6     37 7
//  38 8     39 9     3a :     3b ;     3c <     3d =     3e >     3f ?
//  40 @     41 A     42 B     43 C     44 D     45 E     46 F     47 G
//  48 H     49 I     4a J     4b K     4c L     4d M     4e N     4f O
//  50 P     51 Q     52 R     53 S     54 T     55 U     56 V     57 W
//  58 X     59 Y     5a Z     5b [     5c \     5d ]     5e ^     5f _
//  60 `     61 a     62 b     63 c     64 d     65 e     66 f     67 g
//  68 h     69 i     6a j     6b k     6c l     6d m     6e n     6f o
//  70 p     71 q     72 r     73 s     74 t     75 u     76 v     77 w
//  78 x     79 y     7a z     7b {     7c |     7d }     7e ~     7f del
//---------------------------------------------------------------------

//----- Execute a Vi Command -----------------------------------
static void do_cmd(char c)
{
	const char *msg;
	char c1, *p, *q, buf[9], *save_dot;
	int cnt, i, j, dir, yf;

	c1 = c;				// quiet the compiler
	cnt = yf = dir = 0;	// quiet the compiler
	msg = p = q = save_dot = buf;	// quiet the compiler
	memset(buf, '\0', 9);	// clear buf

	show_status_line();

	/* if this is a cursor key, skip these checks */
	switch (c) {
		case VI_K_UP:
		case VI_K_DOWN:
		case VI_K_LEFT:
		case VI_K_RIGHT:
		case VI_K_HOME:
		case VI_K_END:
		case VI_K_PAGEUP:
		case VI_K_PAGEDOWN:
			goto key_cmd_mode;
	}

	if (cmd_mode == 2) {
		//  flip-flop Insert/Replace mode
		if (c == VI_K_INSERT)
			goto dc_i;
		// we are 'R'eplacing the current *dot with new char
		if (*dot == '\n') {
			// don't Replace past E-o-l
			cmd_mode = 1;	// convert to insert
		} else {
			if (1 <= c || Isprint(c)) {
				if (c != 27)
					dot = yank_delete(dot, dot, 0, YANKDEL);	// delete char
				dot = char_insert(dot, c);	// insert new char
			}
			goto dc1;
		}
	}
	if (cmd_mode == 1) {
		//  hitting "Insert" twice means "R" replace mode
		if (c == VI_K_INSERT) goto dc5;
		// insert the char c at "dot"
		if (1 <= c || Isprint(c)) {
			dot = char_insert(dot, c);
		}
		goto dc1;
	}

 key_cmd_mode:
	switch (c) {
		//case 0x01:	// soh
		//case 0x09:	// ht
		//case 0x0b:	// vt
		//case 0x0e:	// so
		//case 0x0f:	// si
		//case 0x10:	// dle
		//case 0x11:	// dc1
		//case 0x13:	// dc3
#if ENABLE_FEATURE_VI_CRASHME
	case 0x14:			// dc4  ctrl-T
		crashme = (crashme == 0) ? 1 : 0;
		break;
#endif
		//case 0x16:	// syn
		//case 0x17:	// etb
		//case 0x18:	// can
		//case 0x1c:	// fs
		//case 0x1d:	// gs
		//case 0x1e:	// rs
		//case 0x1f:	// us
		//case '!':	// !-
		//case '#':	// #-
		//case '&':	// &-
		//case '(':	// (-
		//case ')':	// )-
		//case '*':	// *-
		//case ',':	// ,-
		//case '=':	// =-
		//case '@':	// @-
		//case 'F':	// F-
		//case 'K':	// K-
		//case 'Q':	// Q-
		//case 'S':	// S-
		//case 'T':	// T-
		//case 'V':	// V-
		//case '[':	// [-
		//case '\\':	// \-
		//case ']':	// ]-
		//case '_':	// _-
		//case '`':	// `-
		//case 'g':	// g-
		//case 'u':	// u- FIXME- there is no undo
		//case 'v':	// v-
	default:			// unrecognised command
		buf[0] = c;
		buf[1] = '\0';
		if (c < ' ') {
			buf[0] = '^';
			buf[1] = c + '@';
			buf[2] = '\0';
		}
		ni(buf);
		end_cmd_q();	// stop adding to q
	case 0x00:			// nul- ignore
		break;
	case 2:			// ctrl-B  scroll up   full screen
	case VI_K_PAGEUP:	// Cursor Key Page Up
		dot_scroll(rows - 2, -1);
		break;
#if ENABLE_FEATURE_VI_USE_SIGNALS
	case 0x03:			// ctrl-C   interrupt
		longjmp(restart, 1);
		break;
	case 26:			// ctrl-Z suspend
		suspend_sig(SIGTSTP);
		break;
#endif
	case 4:			// ctrl-D  scroll down half screen
		dot_scroll((rows - 2) / 2, 1);
		break;
	case 5:			// ctrl-E  scroll down one line
		dot_scroll(1, 1);
		break;
	case 6:			// ctrl-F  scroll down full screen
	case VI_K_PAGEDOWN:	// Cursor Key Page Down
		dot_scroll(rows - 2, 1);
		break;
	case 7:			// ctrl-G  show current status
		last_status_cksum = 0;	// force status update
		break;
	case 'h':			// h- move left
	case VI_K_LEFT:	// cursor key Left
	case 8:		// ctrl-H- move left    (This may be ERASE char)
	case 0x7f:	// DEL- move left   (This may be ERASE char)
		if (cmdcnt-- > 1) {
			do_cmd(c);
		}				// repeat cnt
		dot_left();
		break;
	case 10:			// Newline ^J
	case 'j':			// j- goto next line, same col
	case VI_K_DOWN:	// cursor key Down
		if (cmdcnt-- > 1) {
			do_cmd(c);
		}				// repeat cnt
		dot_next();		// go to next B-o-l
		dot = move_to_col(dot, ccol + offset);	// try stay in same col
		break;
	case 12:			// ctrl-L  force redraw whole screen
	case 18:			// ctrl-R  force redraw
		place_cursor(0, 0, FALSE);	// put cursor in correct place
		clear_to_eos();	// tel terminal to erase display
		mysleep(10);
		screen_erase();	// erase the internal screen buffer
		last_status_cksum = 0;	// force status update
		refresh(TRUE);	// this will redraw the entire display
		break;
	case 13:			// Carriage Return ^M
	case '+':			// +- goto next line
		if (cmdcnt-- > 1) {
			do_cmd(c);
		}				// repeat cnt
		dot_next();
		dot_skip_over_ws();
		break;
	case 21:			// ctrl-U  scroll up   half screen
		dot_scroll((rows - 2) / 2, -1);
		break;
	case 25:			// ctrl-Y  scroll up one line
		dot_scroll(1, -1);
		break;
	case 27:			// esc
		if (cmd_mode == 0)
			indicate_error(c);
		cmd_mode = 0;	// stop insrting
		end_cmd_q();
		last_status_cksum = 0;	// force status update
		break;
	case ' ':			// move right
	case 'l':			// move right
	case VI_K_RIGHT:	// Cursor Key Right
		if (cmdcnt-- > 1) {
			do_cmd(c);
		}				// repeat cnt
		dot_right();
		break;
#if ENABLE_FEATURE_VI_YANKMARK
	case '"':			// "- name a register to use for Delete/Yank
		c1 = get_one_char();
		c1 = tolower(c1);
		if (islower(c1)) {
			YDreg = c1 - 'a';
		} else {
			indicate_error(c);
		}
		break;
	case '\'':			// '- goto a specific mark
		c1 = get_one_char();
		c1 = tolower(c1);
		if (islower(c1)) {
			c1 = c1 - 'a';
			// get the b-o-l
			q = mark[(unsigned char) c1];
			if (text <= q && q < end) {
				dot = q;
				dot_begin();	// go to B-o-l
				dot_skip_over_ws();
			}
		} else if (c1 == '\'') {	// goto previous context
			dot = swap_context(dot);	// swap current and previous context
			dot_begin();	// go to B-o-l
			dot_skip_over_ws();
		} else {
			indicate_error(c);
		}
		break;
	case 'm':			// m- Mark a line
		// this is really stupid.  If there are any inserts or deletes
		// between text[0] and dot then this mark will not point to the
		// correct location! It could be off by many lines!
		// Well..., at least its quick and dirty.
		c1 = get_one_char();
		c1 = tolower(c1);
		if (islower(c1)) {
			c1 = c1 - 'a';
			// remember the line
			mark[(int) c1] = dot;
		} else {
			indicate_error(c);
		}
		break;
	case 'P':			// P- Put register before
	case 'p':			// p- put register after
		p = reg[YDreg];
		if (p == 0) {
			psbs("Nothing in register %c", what_reg());
			break;
		}
		// are we putting whole lines or strings
		if (strchr(p, '\n') != NULL) {
			if (c == 'P') {
				dot_begin();	// putting lines- Put above
			}
			if (c == 'p') {
				// are we putting after very last line?
				if (end_line(dot) == (end - 1)) {
					dot = end;	// force dot to end of text[]
				} else {
					dot_next();	// next line, then put before
				}
			}
		} else {
			if (c == 'p')
				dot_right();	// move to right, can move to NL
		}
		dot = string_insert(dot, p);	// insert the string
		end_cmd_q();	// stop adding to q
		break;
	case 'U':			// U- Undo; replace current line with original version
		if (reg[Ureg] != 0) {
			p = begin_line(dot);
			q = end_line(dot);
			p = text_hole_delete(p, q);	// delete cur line
			p = string_insert(p, reg[Ureg]);	// insert orig line
			dot = p;
			dot_skip_over_ws();
		}
		break;
#endif /* FEATURE_VI_YANKMARK */
	case '$':			// $- goto end of line
	case VI_K_END:		// Cursor Key End
		if (cmdcnt-- > 1) {
			do_cmd(c);
		}				// repeat cnt
		dot = end_line(dot);
		break;
	case '%':			// %- find matching char of pair () [] {}
		for (q = dot; q < end && *q != '\n'; q++) {
			if (strchr("()[]{}", *q) != NULL) {
				// we found half of a pair
				p = find_pair(q, *q);
				if (p == NULL) {
					indicate_error(c);
				} else {
					dot = p;
				}
				break;
			}
		}
		if (*q == '\n')
			indicate_error(c);
		break;
	case 'f':			// f- forward to a user specified char
		last_forward_char = get_one_char();	// get the search char
		//
		// dont separate these two commands. 'f' depends on ';'
		//
		//**** fall thru to ... ';'
	case ';':			// ;- look at rest of line for last forward char
		if (cmdcnt-- > 1) {
			do_cmd(';');
		}				// repeat cnt
		if (last_forward_char == 0)
			break;
		q = dot + 1;
		while (q < end - 1 && *q != '\n' && *q != last_forward_char) {
			q++;
		}
		if (*q == last_forward_char)
			dot = q;
		break;
	case '-':			// -- goto prev line
		if (cmdcnt-- > 1) {
			do_cmd(c);
		}				// repeat cnt
		dot_prev();
		dot_skip_over_ws();
		break;
#if ENABLE_FEATURE_VI_DOT_CMD
	case '.':			// .- repeat the last modifying command
		// Stuff the last_modifying_cmd back into stdin
		// and let it be re-executed.
		if (last_modifying_cmd != 0) {
			ioq = ioq_start = xstrdup(last_modifying_cmd);
		}
		break;
#endif
#if ENABLE_FEATURE_VI_SEARCH
	case '?':			// /- search for a pattern
	case '/':			// /- search for a pattern
		buf[0] = c;
		buf[1] = '\0';
		q = get_input_line(buf);	// get input line- use "status line"
		if (q[0] && !q[1])
			goto dc3; // if no pat re-use old pat
		if (q[0]) {       // strlen(q) > 1: new pat- save it and find
			// there is a new pat
			free(last_search_pattern);
			last_search_pattern = xstrdup(q);
			goto dc3;	// now find the pattern
		}
		// user changed mind and erased the "/"-  do nothing
		break;
	case 'N':			// N- backward search for last pattern
		if (cmdcnt-- > 1) {
			do_cmd(c);
		}				// repeat cnt
		dir = BACK;		// assume BACKWARD search
		p = dot - 1;
		if (last_search_pattern[0] == '?') {
			dir = FORWARD;
			p = dot + 1;
		}
		goto dc4;		// now search for pattern
		break;
	case 'n':			// n- repeat search for last pattern
		// search rest of text[] starting at next char
		// if search fails return orignal "p" not the "p+1" address
		if (cmdcnt-- > 1) {
			do_cmd(c);
		}				// repeat cnt
 dc3:
		if (last_search_pattern == 0) {
			msg = "No previous regular expression";
			goto dc2;
		}
		if (last_search_pattern[0] == '/') {
			dir = FORWARD;	// assume FORWARD search
			p = dot + 1;
		}
		if (last_search_pattern[0] == '?') {
			dir = BACK;
			p = dot - 1;
		}
 dc4:
		q = char_search(p, last_search_pattern + 1, dir, FULL);
		if (q != NULL) {
			dot = q;	// good search, update "dot"
			msg = "";
			goto dc2;
		}
		// no pattern found between "dot" and "end"- continue at top
		p = text;
		if (dir == BACK) {
			p = end - 1;
		}
		q = char_search(p, last_search_pattern + 1, dir, FULL);
		if (q != NULL) {	// found something
			dot = q;	// found new pattern- goto it
			msg = "search hit BOTTOM, continuing at TOP";
			if (dir == BACK) {
				msg = "search hit TOP, continuing at BOTTOM";
			}
		} else {
			msg = "Pattern not found";
		}
 dc2:
		if (*msg)
			psbs("%s", msg);
		break;
	case '{':			// {- move backward paragraph
		q = char_search(dot, "\n\n", BACK, FULL);
		if (q != NULL) {	// found blank line
			dot = next_line(q);	// move to next blank line
		}
		break;
	case '}':			// }- move forward paragraph
		q = char_search(dot, "\n\n", FORWARD, FULL);
		if (q != NULL) {	// found blank line
			dot = next_line(q);	// move to next blank line
		}
		break;
#endif /* FEATURE_VI_SEARCH */
	case '0':			// 0- goto begining of line
	case '1':			// 1-
	case '2':			// 2-
	case '3':			// 3-
	case '4':			// 4-
	case '5':			// 5-
	case '6':			// 6-
	case '7':			// 7-
	case '8':			// 8-
	case '9':			// 9-
		if (c == '0' && cmdcnt < 1) {
			dot_begin();	// this was a standalone zero
		} else {
			cmdcnt = cmdcnt * 10 + (c - '0');	// this 0 is part of a number
		}
		break;
	case ':':			// :- the colon mode commands
		p = get_input_line(":");	// get input line- use "status line"
#if ENABLE_FEATURE_VI_COLON
		colon(p);		// execute the command
#else
		if (*p == ':')
			p++;				// move past the ':'
		cnt = strlen(p);
		if (cnt <= 0)
			break;
		if (strncasecmp(p, "quit", cnt) == 0
		 || strncasecmp(p, "q!", cnt) == 0   // delete lines
		) {
			if (file_modified && p[1] != '!') {
				psbs("No write since last change (:quit! overrides)");
			} else {
				editing = 0;
			}
		} else if (strncasecmp(p, "write", cnt) == 0
		        || strncasecmp(p, "wq", cnt) == 0
		        || strncasecmp(p, "wn", cnt) == 0
		        || strncasecmp(p, "x", cnt) == 0
		) {
			cnt = file_write(cfn, text, end - 1);
			if (cnt < 0) {
				if (cnt == -1)
					psbs("Write error: %s", strerror(errno));
			} else {
				file_modified = 0;
				last_file_modified = -1;
				psb("\"%s\" %dL, %dC", cfn, count_lines(text, end - 1), cnt);
				if (p[0] == 'x' || p[1] == 'q' || p[1] == 'n'
				 || p[0] == 'X' || p[1] == 'Q' || p[1] == 'N'
				) {
					editing = 0;
				}
			}
		} else if (strncasecmp(p, "file", cnt) == 0 ) {
			last_status_cksum = 0;	// force status update
		} else if (sscanf(p, "%d", &j) > 0) {
			dot = find_line(j);		// go to line # j
			dot_skip_over_ws();
		} else {		// unrecognised cmd
			ni(p);
		}
#endif /* !FEATURE_VI_COLON */
		break;
	case '<':			// <- Left  shift something
	case '>':			// >- Right shift something
		cnt = count_lines(text, dot);	// remember what line we are on
		c1 = get_one_char();	// get the type of thing to delete
		find_range(&p, &q, c1);
		yank_delete(p, q, 1, YANKONLY);	// save copy before change
		p = begin_line(p);
		q = end_line(q);
		i = count_lines(p, q);	// # of lines we are shifting
		for ( ; i > 0; i--, p = next_line(p)) {
			if (c == '<') {
				// shift left- remove tab or 8 spaces
				if (*p == '\t') {
					// shrink buffer 1 char
					text_hole_delete(p, p);
				} else if (*p == ' ') {
					// we should be calculating columns, not just SPACE
					for (j = 0; *p == ' ' && j < tabstop; j++) {
						text_hole_delete(p, p);
					}
				}
			} else if (c == '>') {
				// shift right -- add tab or 8 spaces
				char_insert(p, '\t');
			}
		}
		dot = find_line(cnt);	// what line were we on
		dot_skip_over_ws();
		end_cmd_q();	// stop adding to q
		break;
	case 'A':			// A- append at e-o-l
		dot_end();		// go to e-o-l
		//**** fall thru to ... 'a'
	case 'a':			// a- append after current char
		if (*dot != '\n')
			dot++;
		goto dc_i;
		break;
	case 'B':			// B- back a blank-delimited Word
	case 'E':			// E- end of a blank-delimited word
	case 'W':			// W- forward a blank-delimited word
		if (cmdcnt-- > 1) {
			do_cmd(c);
		}				// repeat cnt
		dir = FORWARD;
		if (c == 'B')
			dir = BACK;
		if (c == 'W' || isspace(dot[dir])) {
			dot = skip_thing(dot, 1, dir, S_TO_WS);
			dot = skip_thing(dot, 2, dir, S_OVER_WS);
		}
		if (c != 'W')
			dot = skip_thing(dot, 1, dir, S_BEFORE_WS);
		break;
	case 'C':			// C- Change to e-o-l
	case 'D':			// D- delete to e-o-l
		save_dot = dot;
		dot = dollar_line(dot);	// move to before NL
		// copy text into a register and delete
		dot = yank_delete(save_dot, dot, 0, YANKDEL);	// delete to e-o-l
		if (c == 'C')
			goto dc_i;	// start inserting
#if ENABLE_FEATURE_VI_DOT_CMD
		if (c == 'D')
			end_cmd_q();	// stop adding to q
#endif
		break;
	case 'G':		// G- goto to a line number (default= E-O-F)
		dot = end - 1;				// assume E-O-F
		if (cmdcnt > 0) {
			dot = find_line(cmdcnt);	// what line is #cmdcnt
		}
		dot_skip_over_ws();
		break;
	case 'H':			// H- goto top line on screen
		dot = screenbegin;
		if (cmdcnt > (rows - 1)) {
			cmdcnt = (rows - 1);
		}
		if (cmdcnt-- > 1) {
			do_cmd('+');
		}				// repeat cnt
		dot_skip_over_ws();
		break;
	case 'I':			// I- insert before first non-blank
		dot_begin();	// 0
		dot_skip_over_ws();
		//**** fall thru to ... 'i'
	case 'i':			// i- insert before current char
	case VI_K_INSERT:	// Cursor Key Insert
 dc_i:
		cmd_mode = 1;	// start insrting
		break;
	case 'J':			// J- join current and next lines together
		if (cmdcnt-- > 2) {
			do_cmd(c);
		}				// repeat cnt
		dot_end();		// move to NL
		if (dot < end - 1) {	// make sure not last char in text[]
			*dot++ = ' ';	// replace NL with space
			file_modified++;
			while (isblnk(*dot)) {	// delete leading WS
				dot_delete();
			}
		}
		end_cmd_q();	// stop adding to q
		break;
	case 'L':			// L- goto bottom line on screen
		dot = end_screen();
		if (cmdcnt > (rows - 1)) {
			cmdcnt = (rows - 1);
		}
		if (cmdcnt-- > 1) {
			do_cmd('-');
		}				// repeat cnt
		dot_begin();
		dot_skip_over_ws();
		break;
	case 'M':			// M- goto middle line on screen
		dot = screenbegin;
		for (cnt = 0; cnt < (rows-1) / 2; cnt++)
			dot = next_line(dot);
		break;
	case 'O':			// O- open a empty line above
		//    0i\n ESC -i
		p = begin_line(dot);
		if (p[-1] == '\n') {
			dot_prev();
	case 'o':			// o- open a empty line below; Yes, I know it is in the middle of the "if (..."
			dot_end();
			dot = char_insert(dot, '\n');
		} else {
			dot_begin();	// 0
			dot = char_insert(dot, '\n');	// i\n ESC
			dot_prev();	// -
		}
		goto dc_i;
		break;
	case 'R':			// R- continuous Replace char
 dc5:
		cmd_mode = 2;
		break;
	case 'X':			// X- delete char before dot
	case 'x':			// x- delete the current char
	case 's':			// s- substitute the current char
		if (cmdcnt-- > 1) {
			do_cmd(c);
		}				// repeat cnt
		dir = 0;
		if (c == 'X')
			dir = -1;
		if (dot[dir] != '\n') {
			if (c == 'X')
				dot--;	// delete prev char
			dot = yank_delete(dot, dot, 0, YANKDEL);	// delete char
		}
		if (c == 's')
			goto dc_i;	// start insrting
		end_cmd_q();	// stop adding to q
		break;
	case 'Z':			// Z- if modified, {write}; exit
		// ZZ means to save file (if necessary), then exit
		c1 = get_one_char();
		if (c1 != 'Z') {
			indicate_error(c);
			break;
		}
		if (file_modified) {
#if ENABLE_FEATURE_VI_READONLY
			if (vi_readonly || readonly) {
				psbs("\"%s\" File is read only", cfn);
				break;
			}
#endif
			cnt = file_write(cfn, text, end - 1);
			if (cnt < 0) {
				if (cnt == -1)
					psbs("Write error: %s", strerror(errno));
			} else if (cnt == (end - 1 - text + 1)) {
				editing = 0;
			}
		} else {
			editing = 0;
		}
		break;
	case '^':			// ^- move to first non-blank on line
		dot_begin();
		dot_skip_over_ws();
		break;
	case 'b':			// b- back a word
	case 'e':			// e- end of word
		if (cmdcnt-- > 1) {
			do_cmd(c);
		}				// repeat cnt
		dir = FORWARD;
		if (c == 'b')
			dir = BACK;
		if ((dot + dir) < text || (dot + dir) > end - 1)
			break;
		dot += dir;
		if (isspace(*dot)) {
			dot = skip_thing(dot, (c == 'e') ? 2 : 1, dir, S_OVER_WS);
		}
		if (isalnum(*dot) || *dot == '_') {
			dot = skip_thing(dot, 1, dir, S_END_ALNUM);
		} else if (ispunct(*dot)) {
			dot = skip_thing(dot, 1, dir, S_END_PUNCT);
		}
		break;
	case 'c':			// c- change something
	case 'd':			// d- delete something
#if ENABLE_FEATURE_VI_YANKMARK
	case 'y':			// y- yank   something
	case 'Y':			// Y- Yank a line
#endif
		yf = YANKDEL;	// assume either "c" or "d"
#if ENABLE_FEATURE_VI_YANKMARK
		if (c == 'y' || c == 'Y')
			yf = YANKONLY;
#endif
		c1 = 'y';
		if (c != 'Y')
			c1 = get_one_char();	// get the type of thing to delete
		find_range(&p, &q, c1);
		if (c1 == 27) {	// ESC- user changed mind and wants out
			c = c1 = 27;	// Escape- do nothing
		} else if (strchr("wW", c1)) {
			if (c == 'c') {
				// don't include trailing WS as part of word
				while (isblnk(*q)) {
					if (q <= text || q[-1] == '\n')
						break;
					q--;
				}
			}
			dot = yank_delete(p, q, 0, yf);	// delete word
		} else if (strchr("^0bBeEft$", c1)) {
			// single line copy text into a register and delete
			dot = yank_delete(p, q, 0, yf);	// delete word
		} else if (strchr("cdykjHL%+-{}\r\n", c1)) {
			// multiple line copy text into a register and delete
			dot = yank_delete(p, q, 1, yf);	// delete lines
			if (c == 'c') {
				dot = char_insert(dot, '\n');
				// on the last line of file don't move to prev line
				if (dot != (end-1)) {
					dot_prev();
				}
			} else if (c == 'd') {
				dot_begin();
				dot_skip_over_ws();
			}
		} else {
			// could not recognize object
			c = c1 = 27;	// error-
			indicate_error(c);
		}
		if (c1 != 27) {
			// if CHANGING, not deleting, start inserting after the delete
			if (c == 'c') {
				strcpy(buf, "Change");
				goto dc_i;	// start inserting
			}
			if (c == 'd') {
				strcpy(buf, "Delete");
			}
#if ENABLE_FEATURE_VI_YANKMARK
			if (c == 'y' || c == 'Y') {
				strcpy(buf, "Yank");
			}
			p = reg[YDreg];
			q = p + strlen(p);
			for (cnt = 0; p <= q; p++) {
				if (*p == '\n')
					cnt++;
			}
			psb("%s %d lines (%d chars) using [%c]",
				buf, cnt, strlen(reg[YDreg]), what_reg());
#endif
			end_cmd_q();	// stop adding to q
		}
		break;
	case 'k':			// k- goto prev line, same col
	case VI_K_UP:		// cursor key Up
		if (cmdcnt-- > 1) {
			do_cmd(c);
		}				// repeat cnt
		dot_prev();
		dot = move_to_col(dot, ccol + offset);	// try stay in same col
		break;
	case 'r':			// r- replace the current char with user input
		c1 = get_one_char();	// get the replacement char
		if (*dot != '\n') {
			*dot = c1;
			file_modified++;	// has the file been modified
		}
		end_cmd_q();	// stop adding to q
		break;
	case 't':			// t- move to char prior to next x
		last_forward_char = get_one_char();
		do_cmd(';');
		if (*dot == last_forward_char)
			dot_left();
		last_forward_char= 0;
		break;
	case 'w':			// w- forward a word
		if (cmdcnt-- > 1) {
			do_cmd(c);
		}				// repeat cnt
		if (isalnum(*dot) || *dot == '_') {	// we are on ALNUM
			dot = skip_thing(dot, 1, FORWARD, S_END_ALNUM);
		} else if (ispunct(*dot)) {	// we are on PUNCT
			dot = skip_thing(dot, 1, FORWARD, S_END_PUNCT);
		}
		if (dot < end - 1)
			dot++;		// move over word
		if (isspace(*dot)) {
			dot = skip_thing(dot, 2, FORWARD, S_OVER_WS);
		}
		break;
	case 'z':			// z-
		c1 = get_one_char();	// get the replacement char
		cnt = 0;
		if (c1 == '.')
			cnt = (rows - 2) / 2;	// put dot at center
		if (c1 == '-')
			cnt = rows - 2;	// put dot at bottom
		screenbegin = begin_line(dot);	// start dot at top
		dot_scroll(cnt, -1);
		break;
	case '|':			// |- move to column "cmdcnt"
		dot = move_to_col(dot, cmdcnt - 1);	// try to move to column
		break;
	case '~':			// ~- flip the case of letters   a-z -> A-Z
		if (cmdcnt-- > 1) {
			do_cmd(c);
		}				// repeat cnt
		if (islower(*dot)) {
			*dot = toupper(*dot);
			file_modified++;	// has the file been modified
		} else if (isupper(*dot)) {
			*dot = tolower(*dot);
			file_modified++;	// has the file been modified
		}
		dot_right();
		end_cmd_q();	// stop adding to q
		break;
		//----- The Cursor and Function Keys -----------------------------
	case VI_K_HOME:	// Cursor Key Home
		dot_begin();
		break;
		// The Fn keys could point to do_macro which could translate them
	case VI_K_FUN1:	// Function Key F1
	case VI_K_FUN2:	// Function Key F2
	case VI_K_FUN3:	// Function Key F3
	case VI_K_FUN4:	// Function Key F4
	case VI_K_FUN5:	// Function Key F5
	case VI_K_FUN6:	// Function Key F6
	case VI_K_FUN7:	// Function Key F7
	case VI_K_FUN8:	// Function Key F8
	case VI_K_FUN9:	// Function Key F9
	case VI_K_FUN10:	// Function Key F10
	case VI_K_FUN11:	// Function Key F11
	case VI_K_FUN12:	// Function Key F12
		break;
	}

 dc1:
	// if text[] just became empty, add back an empty line
	if (end == text) {
		char_insert(text, '\n');	// start empty buf with dummy line
		dot = text;
	}
	// it is OK for dot to exactly equal to end, otherwise check dot validity
	if (dot != end) {
		dot = bound_dot(dot);	// make sure "dot" is valid
	}
#if ENABLE_FEATURE_VI_YANKMARK
	check_context(c);	// update the current context
#endif

	if (!isdigit(c))
		cmdcnt = 0;		// cmd was not a number, reset cmdcnt
	cnt = dot - begin_line(dot);
	// Try to stay off of the Newline
	if (*dot == '\n' && cnt > 0 && cmd_mode == 0)
		dot--;
}

#if ENABLE_FEATURE_VI_CRASHME
static int totalcmds = 0;
static int Mp = 85;             // Movement command Probability
static int Np = 90;             // Non-movement command Probability
static int Dp = 96;             // Delete command Probability
static int Ip = 97;             // Insert command Probability
static int Yp = 98;             // Yank command Probability
static int Pp = 99;             // Put command Probability
static int M = 0, N = 0, I = 0, D = 0, Y = 0, P = 0, U = 0;
const char chars[20] = "\t012345 abcdABCD-=.$";
const char *const words[20] = {
	"this", "is", "a", "test",
	"broadcast", "the", "emergency", "of",
	"system", "quick", "brown", "fox",
	"jumped", "over", "lazy", "dogs",
	"back", "January", "Febuary", "March"
};
const char *const lines[20] = {
	"You should have received a copy of the GNU General Public License\n",
	"char c, cm, *cmd, *cmd1;\n",
	"generate a command by percentages\n",
	"Numbers may be typed as a prefix to some commands.\n",
	"Quit, discarding changes!\n",
	"Forced write, if permission originally not valid.\n",
	"In general, any ex or ed command (such as substitute or delete).\n",
	"I have tickets available for the Blazers vs LA Clippers for Monday, Janurary 1 at 1:00pm.\n",
	"Please get w/ me and I will go over it with you.\n",
	"The following is a list of scheduled, committed changes.\n",
	"1.   Launch Norton Antivirus (Start, Programs, Norton Antivirus)\n",
	"Reminder....Town Meeting in Central Perk cafe today at 3:00pm.\n",
	"Any question about transactions please contact Sterling Huxley.\n",
	"I will try to get back to you by Friday, December 31.\n",
	"This Change will be implemented on Friday.\n",
	"Let me know if you have problems accessing this;\n",
	"Sterling Huxley recently added you to the access list.\n",
	"Would you like to go to lunch?\n",
	"The last command will be automatically run.\n",
	"This is too much english for a computer geek.\n",
};
char *multilines[20] = {
	"You should have received a copy of the GNU General Public License\n",
	"char c, cm, *cmd, *cmd1;\n",
	"generate a command by percentages\n",
	"Numbers may be typed as a prefix to some commands.\n",
	"Quit, discarding changes!\n",
	"Forced write, if permission originally not valid.\n",
	"In general, any ex or ed command (such as substitute or delete).\n",
	"I have tickets available for the Blazers vs LA Clippers for Monday, Janurary 1 at 1:00pm.\n",
	"Please get w/ me and I will go over it with you.\n",
	"The following is a list of scheduled, committed changes.\n",
	"1.   Launch Norton Antivirus (Start, Programs, Norton Antivirus)\n",
	"Reminder....Town Meeting in Central Perk cafe today at 3:00pm.\n",
	"Any question about transactions please contact Sterling Huxley.\n",
	"I will try to get back to you by Friday, December 31.\n",
	"This Change will be implemented on Friday.\n",
	"Let me know if you have problems accessing this;\n",
	"Sterling Huxley recently added you to the access list.\n",
	"Would you like to go to lunch?\n",
	"The last command will be automatically run.\n",
	"This is too much english for a computer geek.\n",
};

// create a random command to execute
static void crash_dummy()
{
	static int sleeptime;   // how long to pause between commands
	char c, cm, *cmd, *cmd1;
	int i, cnt, thing, rbi, startrbi, percent;

	// "dot" movement commands
	cmd1 = " \n\r\002\004\005\006\025\0310^$-+wWeEbBhjklHL";

	// is there already a command running?
	if (readed_for_parse > 0)
		goto cd1;
 cd0:
	startrbi = rbi = 0;
	sleeptime = 0;          // how long to pause between commands
	memset(readbuffer, '\0', BUFSIZ);   // clear the read buffer
	// generate a command by percentages
	percent = (int) lrand48() % 100;        // get a number from 0-99
	if (percent < Mp) {     //  Movement commands
		// available commands
		cmd = cmd1;
		M++;
	} else if (percent < Np) {      //  non-movement commands
		cmd = "mz<>\'\"";       // available commands
		N++;
	} else if (percent < Dp) {      //  Delete commands
		cmd = "dx";             // available commands
		D++;
	} else if (percent < Ip) {      //  Inset commands
		cmd = "iIaAsrJ";        // available commands
		I++;
	} else if (percent < Yp) {      //  Yank commands
		cmd = "yY";             // available commands
		Y++;
	} else if (percent < Pp) {      //  Put commands
		cmd = "pP";             // available commands
		P++;
	} else {
		// We do not know how to handle this command, try again
		U++;
		goto cd0;
	}
	// randomly pick one of the available cmds from "cmd[]"
	i = (int) lrand48() % strlen(cmd);
	cm = cmd[i];
	if (strchr(":\024", cm))
		goto cd0;               // dont allow colon or ctrl-T commands
	readbuffer[rbi++] = cm; // put cmd into input buffer

	// now we have the command-
	// there are 1, 2, and multi char commands
	// find out which and generate the rest of command as necessary
	if (strchr("dmryz<>\'\"", cm)) {        // 2-char commands
		cmd1 = " \n\r0$^-+wWeEbBhjklHL";
		if (cm == 'm' || cm == '\'' || cm == '\"') {    // pick a reg[]
			cmd1 = "abcdefghijklmnopqrstuvwxyz";
		}
		thing = (int) lrand48() % strlen(cmd1); // pick a movement command
		c = cmd1[thing];
		readbuffer[rbi++] = c;  // add movement to input buffer
	}
	if (strchr("iIaAsc", cm)) {     // multi-char commands
		if (cm == 'c') {
			// change some thing
			thing = (int) lrand48() % strlen(cmd1); // pick a movement command
			c = cmd1[thing];
			readbuffer[rbi++] = c;  // add movement to input buffer
		}
		thing = (int) lrand48() % 4;    // what thing to insert
		cnt = (int) lrand48() % 10;     // how many to insert
		for (i = 0; i < cnt; i++) {
			if (thing == 0) {       // insert chars
				readbuffer[rbi++] = chars[((int) lrand48() % strlen(chars))];
			} else if (thing == 1) {        // insert words
				strcat(readbuffer, words[(int) lrand48() % 20]);
				strcat(readbuffer, " ");
				sleeptime = 0;  // how fast to type
			} else if (thing == 2) {        // insert lines
				strcat(readbuffer, lines[(int) lrand48() % 20]);
				sleeptime = 0;  // how fast to type
			} else {        // insert multi-lines
				strcat(readbuffer, multilines[(int) lrand48() % 20]);
				sleeptime = 0;  // how fast to type
			}
		}
		strcat(readbuffer, "\033");
	}
	readed_for_parse = strlen(readbuffer);
 cd1:
	totalcmds++;
	if (sleeptime > 0)
		mysleep(sleeptime);      // sleep 1/100 sec
}

// test to see if there are any errors
static void crash_test()
{
	static time_t oldtim;

	time_t tim;
	char d[2], msg[BUFSIZ];

	msg[0] = '\0';
	if (end < text) {
		strcat(msg, "end<text ");
	}
	if (end > textend) {
		strcat(msg, "end>textend ");
	}
	if (dot < text) {
		strcat(msg, "dot<text ");
	}
	if (dot > end) {
		strcat(msg, "dot>end ");
	}
	if (screenbegin < text) {
		strcat(msg, "screenbegin<text ");
	}
	if (screenbegin > end - 1) {
		strcat(msg, "screenbegin>end-1 ");
	}

	if (msg[0]) {
		alarm(0);
		printf("\n\n%d: \'%c\' %s\n\n\n%s[Hit return to continue]%s",
			totalcmds, last_input_char, msg, SOs, SOn);
		fflush(stdout);
		while (read(0, d, 1) > 0) {
			if (d[0] == '\n' || d[0] == '\r')
				break;
		}
		alarm(3);
	}
	tim = (time_t) time((time_t *) 0);
	if (tim >= (oldtim + 3)) {
		sprintf(status_buffer,
				"Tot=%d: M=%d N=%d I=%d D=%d Y=%d P=%d U=%d size=%d",
				totalcmds, M, N, I, D, Y, P, U, end - text + 1);
		oldtim = tim;
	}
}
#endif
