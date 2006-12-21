/* vi: set sw=4 ts=4: */
/*
 * Mini less implementation for busybox
 *
 * Copyright (C) 2005 by Rob Sullivan <cogito.ergo.cogito@gmail.com>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

/*
 * TODO:
 * - Add more regular expression support - search modifiers, certain matches, etc.
 * - Add more complex bracket searching - currently, nested brackets are
 * not considered.
 * - Add support for "F" as an input. This causes less to act in
 * a similar way to tail -f.
 * - Allow horizontal scrolling.
 *
 * Notes:
 * - the inp file pointer is used so that keyboard input works after
 * redirected input has been read from stdin
 */

#include "busybox.h"
#if ENABLE_FEATURE_LESS_REGEXP
#include "xregex.h"
#endif


/* These are the escape sequences corresponding to special keys */
#define REAL_KEY_UP 'A'
#define REAL_KEY_DOWN 'B'
#define REAL_KEY_RIGHT 'C'
#define REAL_KEY_LEFT 'D'
#define REAL_PAGE_UP '5'
#define REAL_PAGE_DOWN '6'
#define REAL_KEY_HOME '7'
#define REAL_KEY_END '8'

/* These are the special codes assigned by this program to the special keys */
#define KEY_UP 20
#define KEY_DOWN 21
#define KEY_RIGHT 22
#define KEY_LEFT 23
#define PAGE_UP 24
#define PAGE_DOWN 25
#define KEY_HOME 26
#define KEY_END 27

/* The escape codes for highlighted and normal text */
#define HIGHLIGHT "\033[7m"
#define NORMAL "\033[0m"
/* The escape code to clear the screen */
#define CLEAR "\033[H\033[J"
/* The escape code to clear to end of line */
#define CLEAR_2_EOL "\033[K"

#define MAXLINES CONFIG_FEATURE_LESS_MAXLINES

static int height;
static int width;
static char **files;
static char *filename;
static const char **buffer;
static char **flines;
static int current_file = 1;
static int line_pos;
static int num_flines;
static int num_files = 1;
static const char *empty_line_marker = "~";

/* Command line options */
#define FLAG_E 1
#define FLAG_M (1<<1)
#define FLAG_m (1<<2)
#define FLAG_N (1<<3)
#define FLAG_TILDE (1<<4)
/* hijack command line options variable for internal state vars */
#define LESS_STATE_MATCH_BACKWARDS (1<<6)

#if ENABLE_FEATURE_LESS_MARKS
static int mark_lines[15][2];
static int num_marks;
#endif

#if ENABLE_FEATURE_LESS_REGEXP
static int *match_lines;
static int match_pos;
static int num_matches;
static regex_t pattern;
static int pattern_valid;
#endif

/* Needed termios structures */
static struct termios term_orig, term_vi;

/* File pointer to get input from */
static FILE *inp;

/* Reset terminal input to normal */
static void set_tty_cooked(void)
{
	fflush(stdout);
	tcsetattr(fileno(inp), TCSANOW, &term_orig);
}

/* Exit the program gracefully */
static void tless_exit(int code)
{
	/* TODO: We really should save the terminal state when we start,
		 and restore it when we exit. Less does this with the
		 "ti" and "te" termcap commands; can this be done with
		 only termios.h? */

	putchar('\n');
	fflush_stdout_and_exit(code);
}

/* Grab a character from input without requiring the return key. If the
   character is ASCII \033, get more characters and assign certain sequences
   special return codes. Note that this function works best with raw input. */
static int tless_getch(void)
{
	int input;
	/* Set terminal input to raw mode (taken from vi.c) */
	tcsetattr(fileno(inp), TCSANOW, &term_vi);
 again:
	input = getc(inp);
	/* Detect escape sequences (i.e. arrow keys) and handle
	   them accordingly */

	if (input == '\033' && getc(inp) == '[') {
		unsigned i;
		input = getc(inp);
		set_tty_cooked();

		i = input - REAL_KEY_UP;
		if (i < 4)
			return 20 + i;
		i = input - REAL_PAGE_UP;
		if (i < 4)
			return 24 + i;
		return 0; /* ?? */
	}
	/* Reject almost all control chars */
	if (input < ' ' && input != 0x0d && input != 8) goto again;
	set_tty_cooked();
	return input;
}

static char* tless_gets(int sz)
{
	int c;
	int i = 0;
	char *result = xzalloc(1);
	while (1) {
		c = tless_getch();
		if (c == 0x0d)
			return result;
		if (c == 0x7f) c = 8;
		if (c == 8 && i) {
			printf("\x8 \x8");
			i--;
		}
		if (c < ' ')
			continue;
		if (i >= width - sz - 1)
			continue; /* len limit */
		putchar(c);
		result[i++] = c;
		result = xrealloc(result, i+1);
		result[i] = '\0';
	}
}

/* Move the cursor to a position (x,y), where (0,0) is the
   top-left corner of the console */
static void move_cursor(int line, int row)
{
	printf("\033[%u;%uH", line, row);
}

static void clear_line(void)
{
	printf("\033[%u;0H" CLEAR_2_EOL, height);
}

static void print_hilite(const char *str)
{
	printf(HIGHLIGHT"%s"NORMAL, str);
}

static void print_statusline(const char *str)
{
	clear_line();
	printf(HIGHLIGHT"%.*s"NORMAL, width-1, str);
}

static void data_readlines(void)
{
	unsigned i, pos;
	unsigned lineno = 1;
	int w = width;
	char terminated, last_terminated = 1;
	char *current_line, *p;
	FILE *fp;

	if (filename)
		fp = xfopen(filename, "r");
	else {
		/* "less" with no arguments in argv[] */
		fp = stdin;
		/* For status line only */
		filename = xstrdup(bb_msg_standard_input);
	}

	flines = NULL;
	if (option_mask32 & FLAG_N) {
		w -= 8;
	}
	for (i = 0; !feof(fp) && i <= MAXLINES; i++) {
		flines = xrealloc(flines, (i+1) * sizeof(char *));
		current_line = xmalloc(w);
 again:
		p = current_line;
		pos = 0;
		terminated = 0;
		while (1) {
			int c = getc(fp);
			if (c == '\t') pos += (pos^7) & 7;
			pos++;
			if (pos >= w) { ungetc(c, fp); break; }
			if (c == EOF) break;
			if (c == '\n') { terminated = 1; break; }
			/* NUL is substituted by '\n'! */
			if (c == '\0') c = '\n';
			*p++ = c;
		}
		*p = '\0';
		if (fp != stdin)
			die_if_ferror(fp, filename);

		/* Corner case: linewrap with only "" wrapping to next line */
		/* Looks ugly on screen, so we do not store this empty line */
		if (!last_terminated && !current_line[0]) {
			last_terminated = 1;
			lineno++;
			goto again;
		}

		last_terminated = terminated;
		if (option_mask32 & FLAG_N) {
			/* Width of 7 preserves tab spacing in the text */
			flines[i] = xasprintf(
				(lineno <= 9999999) ? "%7u %s" : "%07u %s",
				lineno % 10000000, current_line);
			free(current_line);
			if (terminated)
				lineno++;
		} else {
			flines[i] = xrealloc(current_line, strlen(current_line)+1);
		}
	}
	num_flines = i - 1; /* buggie: 'num_flines' must be 'max_fline' */

	/* Reset variables for a new file */

	line_pos = 0;

	fclose(fp);
}

#if ENABLE_FEATURE_LESS_FLAGS

/* Interestingly, writing calc_percent as a function and not a prototype saves around 32 bytes
 * on my build. */
static int calc_percent(void)
{
	return ((100 * (line_pos + height - 2) / num_flines) + 1);
}

/* Print a status line if -M was specified */
static void m_status_print(void)
{
	int percentage;

	clear_line();
	printf(HIGHLIGHT"%s", filename);
	if (num_files > 1)
		printf(" (file %i of %i)", current_file, num_files);
	printf(" lines %i-%i/%i ",
			line_pos + 1, line_pos + height - 1,
			num_flines + 1);
	if (line_pos >= num_flines - height + 2) {
		printf("(END)"NORMAL);
		if (num_files > 1 && current_file != num_files)
			printf(HIGHLIGHT" - next: %s "NORMAL, files[current_file]);
		return;
	}
	percentage = calc_percent();
	printf("%i%% "NORMAL, percentage);
}

#endif

/* Print the status line */
static void status_print(void)
{
	const char *p;

	/* Change the status if flags have been set */
#if ENABLE_FEATURE_LESS_FLAGS
	if (option_mask32 & (FLAG_M|FLAG_m)) {
		m_status_print();
		return;
	}
	/* No flags set */
#endif

	clear_line();
	if (line_pos && line_pos < num_flines - height + 2) {
		putchar(':');
		return;
	}
	p = "(END)";
	if (!line_pos)
		p = filename;
	if (num_files > 1) {
		printf(HIGHLIGHT"%s (file %i of %i) "NORMAL,
				p, current_file, num_files);
		return;
	}
	print_hilite(p);
}

static char controls[] =
	/* NUL: never encountered; TAB: not converted */
	/**/"\x01\x02\x03\x04\x05\x06\x07\x08"  "\x0a\x0b\x0c\x0d\x0e\x0f"
	"\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
	"\x7f\x9b"; /* DEL and infamous Meta-ESC :( */
static char ctrlconv[] =
	/* '\n': it's a former NUL - subst with '@', not 'J' */
	"\x40\x41\x42\x43\x44\x45\x46\x47\x48\x49\x40\x4b\x4c\x4d\x4e\x4f"
	"\x50\x51\x52\x53\x54\x55\x56\x57\x58\x59\x5a\x5b\x5c\x5d\x5e\x5f";

static void print_found(const char *line)
{
	int match_status;
	int eflags;
	char *growline;
	regmatch_t match_structs;

	char buf[width];
	const char *str = line;
	char *p = buf;
	size_t n;

	while (*str) {
		n = strcspn(str, controls);
		if (n) {
			if (!str[n]) break;
			memcpy(p, str, n);
			p += n;
			str += n;
		}
		n = strspn(str, controls);
		memset(p, '.', n);
		p += n;
		str += n;
	}
	strcpy(p, str);

	/* buf[] holds quarantined version of str */

	/* Each part of the line that matches has the HIGHLIGHT
	   and NORMAL escape sequences placed around it.
	   NB: we regex against line, but insert text
	   from quarantined copy (buf[]) */
	str = buf;
	growline = NULL;
	eflags = 0;
	goto start;

	while (match_status == 0) {
		char *new = xasprintf("%s%.*s"HIGHLIGHT"%.*s"NORMAL,
				growline ? : "",
				match_structs.rm_so, str,
				match_structs.rm_eo - match_structs.rm_so,
						str + match_structs.rm_so);
		free(growline); growline = new;
		str += match_structs.rm_eo;
		line += match_structs.rm_eo;
		eflags = REG_NOTBOL;
 start:
		/* Most of the time doesn't find the regex, optimize for that */
		match_status = regexec(&pattern, line, 1, &match_structs, eflags);
	}

	if (!growline) {
		printf(CLEAR_2_EOL"%s\n", str);
		return;
	}
	printf(CLEAR_2_EOL"%s%s\n", growline, str);
	free(growline);
}

static void print_ascii(const char *str)
{
	char buf[width];
	char *p;
	size_t n;

	printf(CLEAR_2_EOL);
	while (*str) {
		n = strcspn(str, controls);
		if (n) {
			if (!str[n]) break;
			printf("%.*s", n, str);
			str += n;
		}
		n = strspn(str, controls);
		p = buf;
		do {
			if (*str == 0x7f)
				*p++ = '?';
			else if (*str == 0x9b)
			/* VT100's CSI, aka Meta-ESC. Who's inventor? */
			/* I want to know who committed this sin */
				*p++ = '{';
			else
				*p++ = ctrlconv[(unsigned char)*str];
			str++;
		} while (--n);
		*p = '\0';
		print_hilite(buf);
	}
	puts(str);
}

/* Print the buffer */
static void buffer_print(void)
{
	int i;

	move_cursor(0, 0);
	for (i = 0; i < height - 1; i++)
		if (pattern_valid)
			print_found(buffer[i]);
		else
			print_ascii(buffer[i]);
	status_print();
}

/* Initialise the buffer */
static void buffer_init(void)
{
	int i;

	/* Fill the buffer until the end of the file or the
	   end of the buffer is reached */
	for (i = 0; i < height - 1 && i <= num_flines; i++) {
		buffer[i] = flines[i];
	}

	/* If the buffer still isn't full, fill it with blank lines */
	for (; i < height - 1; i++) {
		buffer[i] = empty_line_marker;
	}
}

/* Move the buffer up and down in the file in order to scroll */
static void buffer_down(int nlines)
{
	int i;

	if (line_pos + (height - 3) + nlines < num_flines) {
		line_pos += nlines;
		for (i = 0; i < (height - 1); i++) {
			buffer[i] = flines[line_pos + i];
		}
	} else {
		/* As the number of lines requested was too large, we just move
		to the end of the file */
		while (line_pos + (height - 3) + 1 < num_flines) {
			line_pos += 1;
			for (i = 0; i < (height - 1); i++) {
				buffer[i] = flines[line_pos + i];
			}
		}
	}

	/* We exit if the -E flag has been set */
	if ((option_mask32 & FLAG_E) && (line_pos + (height - 2) == num_flines))
		tless_exit(0);
}

static void buffer_up(int nlines)
{
	int i;

	line_pos -= nlines;
	if (line_pos < 0) line_pos = 0;
	for (i = 0; i < height - 1; i++) {
		if (line_pos + i <= num_flines) {
			buffer[i] = flines[line_pos + i];
		} else {
			buffer[i] = empty_line_marker;
		}
	}
}

static void buffer_line(int linenum)
{
	int i;

	if (linenum < 0 || linenum > num_flines) {
		clear_line();
		printf(HIGHLIGHT"%s%u"NORMAL, "Cannot seek to line ", linenum + 1);
		return;
	}

	for (i = 0; i < height - 1; i++) {
		if (linenum + i <= num_flines)
			buffer[i] = flines[linenum + i];
		else {
			buffer[i] = empty_line_marker;
		}
	}
	line_pos = linenum;
	buffer_print();
}

/* Reinitialise everything for a new file - free the memory and start over */
static void reinitialise(void)
{
	int i;

	for (i = 0; i <= num_flines; i++)
		free(flines[i]);
	free(flines);

	data_readlines();
	buffer_init();
	buffer_print();
}

static void examine_file(void)
{
	print_statusline("Examine: ");
	free(filename);
	filename = tless_gets(sizeof("Examine: ")-1);
	/* files start by = argv. why we assume that argv is infinitely long??
	files[num_files] = filename;
	current_file = num_files + 1;
	num_files++; */
	files[0] = filename;
	num_files = current_file = 1;
	reinitialise();
}

/* This function changes the file currently being paged. direction can be one of the following:
 * -1: go back one file
 *  0: go to the first file
 *  1: go forward one file
*/
static void change_file(int direction)
{
	if (current_file != ((direction > 0) ? num_files : 1)) {
		current_file = direction ? current_file + direction : 1;
		free(filename);
		filename = xstrdup(files[current_file - 1]);
		reinitialise();
	} else {
		print_statusline(direction > 0 ? "No next file" : "No previous file");
	}
}

static void remove_current_file(void)
{
	int i;

	if (current_file != 1) {
		change_file(-1);
		for (i = 3; i <= num_files; i++)
			files[i - 2] = files[i - 1];
		num_files--;
		buffer_print();
	} else {
		change_file(1);
		for (i = 2; i <= num_files; i++)
			files[i - 2] = files[i - 1];
		num_files--;
		current_file--;
		buffer_print();
	}
}

static void colon_process(void)
{
	int keypress;

	/* Clear the current line and print a prompt */
	print_statusline(" :");

	keypress = tless_getch();
	switch (keypress) {
		case 'd':
			remove_current_file();
			break;
		case 'e':
			examine_file();
			break;
#if ENABLE_FEATURE_LESS_FLAGS
		case 'f':
			m_status_print();
			break;
#endif
		case 'n':
			change_file(1);
			break;
		case 'p':
			change_file(-1);
			break;
		case 'q':
			tless_exit(0);
			break;
		case 'x':
			change_file(0);
			break;
		default:
			break;
	}
}

static int normalize_match_pos(int match)
{
	match_pos = match;
	if (match >= num_matches)
		match_pos = num_matches - 1;
	if (match < 0)
		return (match_pos = 0);
	return match_pos;
}

#if ENABLE_FEATURE_LESS_REGEXP
static void goto_match(int match)
{
	if (num_matches)
		buffer_line(match_lines[normalize_match_pos(match)]);
}

static void regex_process(void)
{
	char *uncomp_regex, *err;

	/* Reset variables */
	match_lines = xrealloc(match_lines, sizeof(int));
	match_lines[0] = -1;
	match_pos = 0;
	num_matches = 0;
	if (pattern_valid) {
		regfree(&pattern);
		pattern_valid = 0;
	}

	/* Get the uncompiled regular expression from the user */
	clear_line();
	putchar((option_mask32 & LESS_STATE_MATCH_BACKWARDS) ? '?' : '/');
	uncomp_regex = tless_gets(1);
	if (/*!uncomp_regex ||*/ !uncomp_regex[0]) {
		free(uncomp_regex);
		buffer_print();
		return;
	}

	/* Compile the regex and check for errors */
	err = regcomp_or_errmsg(&pattern, uncomp_regex, 0);
	free(uncomp_regex);
	if (err) {
		print_statusline(err);
		free(err);
		return;
	}
	pattern_valid = 1;

	/* Run the regex on each line of the current file */
	for (match_pos = 0; match_pos <= num_flines; match_pos++) {
		if (regexec(&pattern, flines[match_pos], 0, NULL, 0) == 0) {
			match_lines = xrealloc(match_lines, (num_matches+1) * sizeof(int));
			match_lines[num_matches++] = match_pos;
		}
	}

	if (num_matches == 0 || num_flines <= height - 2) {
		buffer_print();
		return;
	}
	for (match_pos = 0; match_pos < num_matches; match_pos++) {
		if (match_lines[match_pos] > line_pos)
			break;
	}
	if (option_mask32 & LESS_STATE_MATCH_BACKWARDS) match_pos--;
	buffer_line(match_lines[normalize_match_pos(match_pos)]);
}
#endif

static void number_process(int first_digit)
{
	int i = 1;
	int num;
	char num_input[sizeof(int)*4]; /* more than enough */
	char keypress;

	num_input[0] = first_digit;

	/* Clear the current line, print a prompt, and then print the digit */
	clear_line();
	printf(":%c", first_digit);

	/* Receive input until a letter is given */
	while (i < sizeof(num_input)-1) {
		num_input[i] = tless_getch();
		if (!num_input[i] || !isdigit(num_input[i]))
			break;
		putchar(num_input[i]);
		i++;
	}

	/* Take the final letter out of the digits string */
	keypress = num_input[i];
	num_input[i] = '\0';
	num = bb_strtou(num_input, NULL, 10);
	/* on format error, num == -1 */
	if (num < 1 || num > MAXLINES) {
		buffer_print();
		return;
	}

	/* We now know the number and the letter entered, so we process them */
	switch (keypress) {
		case KEY_DOWN: case 'z': case 'd': case 'e': case ' ': case '\015':
			buffer_down(num);
			break;
		case KEY_UP: case 'b': case 'w': case 'y': case 'u':
			buffer_up(num);
			break;
		case 'g': case '<': case 'G': case '>':
			if (num_flines >= height - 2)
				buffer_line(num - 1);
			break;
		case 'p': case '%':
			buffer_line(((num / 100) * num_flines) - 1);
			break;
#if ENABLE_FEATURE_LESS_REGEXP
		case 'n':
			goto_match(match_pos + num);
			break;
		case '/':
			option_mask32 &= ~LESS_STATE_MATCH_BACKWARDS;
			regex_process();
			break;
		case '?':
			option_mask32 |= LESS_STATE_MATCH_BACKWARDS;
			regex_process();
			break;
#endif
		default:
			break;
	}
}

#if ENABLE_FEATURE_LESS_FLAGCS
static void flag_change(void)
{
	int keypress;

	clear_line();
	putchar('-');
	keypress = tless_getch();

	switch (keypress) {
		case 'M':
			option_mask32 ^= FLAG_M;
			break;
		case 'm':
			option_mask32 ^= FLAG_m;
			break;
		case 'E':
			option_mask32 ^= FLAG_E;
			break;
		case '~':
			option_mask32 ^= FLAG_TILDE;
			break;
		default:
			break;
	}
}

static void show_flag_status(void)
{
	int keypress;
	int flag_val;

	clear_line();
	putchar('_');
	keypress = tless_getch();

	switch (keypress) {
		case 'M':
			flag_val = option_mask32 & FLAG_M;
			break;
		case 'm':
			flag_val = option_mask32 & FLAG_m;
			break;
		case '~':
			flag_val = option_mask32 & FLAG_TILDE;
			break;
		case 'N':
			flag_val = option_mask32 & FLAG_N;
			break;
		case 'E':
			flag_val = option_mask32 & FLAG_E;
			break;
		default:
			flag_val = 0;
			break;
	}

	clear_line();
	printf(HIGHLIGHT"%s%u"NORMAL, "The status of the flag is: ", flag_val != 0);
}
#endif

static void full_repaint(void)
{
	int temp_line_pos = line_pos;
	data_readlines();
	buffer_init();
	buffer_line(temp_line_pos);
}


static void save_input_to_file(void)
{
	char *current_line;
	int i;
	FILE *fp;

	print_statusline("Log file: ");
	current_line = tless_gets(sizeof("Log file: ")-1);
	if (strlen(current_line) > 0) {
		fp = fopen(current_line, "w");
		free(current_line);
		if (!fp) {
			print_statusline("Error opening log file");
			return;
		}
		for (i = 0; i < num_flines; i++)
			fprintf(fp, "%s\n", flines[i]);
		fclose(fp);
		buffer_print();
		return;
	}
	free(current_line);
	print_statusline("No log file");
}

#if ENABLE_FEATURE_LESS_MARKS
static void add_mark(void)
{
	int letter;

	print_statusline("Mark: ");
	letter = tless_getch();

	if (isalpha(letter)) {

		/* If we exceed 15 marks, start overwriting previous ones */
		if (num_marks == 14)
			num_marks = 0;

		mark_lines[num_marks][0] = letter;
		mark_lines[num_marks][1] = line_pos;
		num_marks++;
	} else {
		print_statusline("Invalid mark letter");
	}
}

static void goto_mark(void)
{
	int letter;
	int i;

	print_statusline("Go to mark: ");
	letter = tless_getch();
	clear_line();

	if (isalpha(letter)) {
		for (i = 0; i <= num_marks; i++)
			if (letter == mark_lines[i][0]) {
				buffer_line(mark_lines[i][1]);
				break;
			}
		if (num_marks == 14 && letter != mark_lines[14][0])
			print_statusline("Mark not set");
	} else
		print_statusline("Invalid mark letter");
}
#endif


#if ENABLE_FEATURE_LESS_BRACKETS

static char opp_bracket(char bracket)
{
	switch (bracket) {
		case '{': case '[':
			return bracket + 2;
		case '(':
			return ')';
		case '}': case ']':
			return bracket - 2;
		case ')':
			return '(';
		default:
			return 0;
	}
}

static void match_right_bracket(char bracket)
{
	int bracket_line = -1;
	int i;

	if (strchr(flines[line_pos], bracket) == NULL) {
		print_statusline("No bracket in top line");
		return;
	}
	for (i = line_pos + 1; i < num_flines; i++) {
		if (strchr(flines[i], opp_bracket(bracket)) != NULL) {
			bracket_line = i;
			break;
		}
	}
	if (bracket_line == -1)
		print_statusline("No matching bracket found");
	buffer_line(bracket_line - height + 2);
}

static void match_left_bracket(char bracket)
{
	int bracket_line = -1;
	int i;

	if (strchr(flines[line_pos + height - 2], bracket) == NULL) {
		print_statusline("No bracket in bottom line");
		return;
	}

	for (i = line_pos + height - 2; i >= 0; i--) {
		if (strchr(flines[i], opp_bracket(bracket)) != NULL) {
			bracket_line = i;
			break;
		}
	}
	if (bracket_line == -1)
		print_statusline("No matching bracket found");
	buffer_line(bracket_line);
}

#endif  /* FEATURE_LESS_BRACKETS */

static void keypress_process(int keypress)
{
	switch (keypress) {
		case KEY_DOWN: case 'e': case 'j': case 0x0d:
			buffer_down(1);
			buffer_print();
			break;
		case KEY_UP: case 'y': case 'k':
			buffer_up(1);
			buffer_print();
			break;
		case PAGE_DOWN: case ' ': case 'z':
			buffer_down(height - 1);
			buffer_print();
			break;
		case PAGE_UP: case 'w': case 'b':
			buffer_up(height - 1);
			buffer_print();
			break;
		case 'd':
			buffer_down((height - 1) / 2);
			buffer_print();
			break;
		case 'u':
			buffer_up((height - 1) / 2);
			buffer_print();
			break;
		case KEY_HOME: case 'g': case 'p': case '<': case '%':
			buffer_line(0);
			break;
		case KEY_END: case 'G': case '>':
			buffer_line(num_flines - height + 2);
			break;
		case 'q': case 'Q':
			tless_exit(0);
			break;
#if ENABLE_FEATURE_LESS_MARKS
		case 'm':
			add_mark();
			buffer_print();
			break;
		case '\'':
			goto_mark();
			buffer_print();
			break;
#endif
		case 'r':
			buffer_print();
			break;
		case 'R':
			full_repaint();
			break;
		case 's':
			save_input_to_file();
			break;
		case 'E':
			examine_file();
			break;
#if ENABLE_FEATURE_LESS_FLAGS
		case '=':
			m_status_print();
			break;
#endif
#if ENABLE_FEATURE_LESS_REGEXP
		case '/':
			option_mask32 &= ~LESS_STATE_MATCH_BACKWARDS;
			regex_process();
			break;
		case 'n':
//printf("HERE 3\n");sleep(1);
			goto_match(match_pos + 1);
			break;
		case 'N':
			goto_match(match_pos - 1);
			break;
		case '?':
			option_mask32 |= LESS_STATE_MATCH_BACKWARDS;
			regex_process();
			break;
#endif
#if ENABLE_FEATURE_LESS_FLAGCS
		case '-':
			flag_change();
			buffer_print();
			break;
		case '_':
			show_flag_status();
			break;
#endif
#if ENABLE_FEATURE_LESS_BRACKETS
		case '{': case '(': case '[':
			match_right_bracket(keypress);
			break;
		case '}': case ')': case ']':
			match_left_bracket(keypress);
			break;
#endif
		case ':':
			colon_process();
			break;
		default:
			break;
	}

	if (isdigit(keypress))
		number_process(keypress);
}

static void sig_catcher(int sig ATTRIBUTE_UNUSED)
{
	set_tty_cooked();
	exit(1);
}

int less_main(int argc, char **argv)
{
	int keypress;

	getopt32(argc, argv, "EMmN~");
	argc -= optind;
	argv += optind;
	files = argv;
	num_files = argc;

	/* Another popular pager, most, detects when stdout
	 * is not a tty and turns into cat. This makes sense. */
	if (!isatty(STDOUT_FILENO))
		return bb_cat(argv);

	if (!num_files) {
		if (isatty(STDIN_FILENO)) {
			/* Just "less"? No args and no redirection? */
			bb_error_msg("missing filename");
			bb_show_usage();
		}
	} else
		filename = xstrdup(files[0]);

	inp = xfopen(CURRENT_TTY, "r");

	get_terminal_width_height(fileno(inp), &width, &height);
	if (width < 10 || height < 3)
		bb_error_msg_and_die("too narrow here");

	buffer = xmalloc(height * sizeof(char *));
	if (option_mask32 & FLAG_TILDE)
		empty_line_marker = "";

	data_readlines();

	tcgetattr(fileno(inp), &term_orig);
	signal(SIGTERM, sig_catcher);
	signal(SIGINT, sig_catcher);
	term_vi = term_orig;
	term_vi.c_lflag &= (~ICANON & ~ECHO);
	term_vi.c_iflag &= (~IXON & ~ICRNL);
	term_vi.c_oflag &= (~ONLCR);
	term_vi.c_cc[VMIN] = 1;
	term_vi.c_cc[VTIME] = 0;

	buffer_init();
	buffer_print();

	while (1) {
		keypress = tless_getch();
		keypress_process(keypress);
	}
}
