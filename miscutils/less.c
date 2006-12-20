/* vi: set sw=4 ts=4: */
/*
 * Mini less implementation for busybox
 *
 * Copyright (C) 2005 by Rob Sullivan <cogito.ergo.cogito@gmail.com>
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

/*
 *      This program needs a lot of development, so consider it in a beta stage
 *      at best.
 *
 *      TODO:
 *      - Add more regular expression support - search modifiers, certain matches, etc.
 *      - Add more complex bracket searching - currently, nested brackets are
 *      not considered.
 *      - Add support for "F" as an input. This causes less to act in
 *      a similar way to tail -f.
 *      - Check for binary files, and prompt the user if a binary file
 *      is detected.
 *      - Allow horizontal scrolling. Currently, lines simply continue onto
 *      the next line, per the terminal's discretion
 *
 *      Notes:
 *      - filename is an array and not a pointer because that avoids all sorts
 *      of complications involving the fact that something that is pointed to
 *      will be changed if the pointer is changed.
 *      - the inp file pointer is used so that keyboard input works after
 *      redirected input has been read from stdin
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

#define MAXLINES CONFIG_FEATURE_LESS_MAXLINES

static int height;
static int width;
static char **files;
static char *filename;
static char **buffer;
static char **flines;
static int current_file = 1;
static int line_pos;
static int num_flines;
static int num_files = 1;

/* Command line options */
#define FLAG_E 1
#define FLAG_M (1<<1)
#define FLAG_m (1<<2)
#define FLAG_N (1<<3)
#define FLAG_TILDE (1<<4)
/* hijack command line options variable for internal state vars */
#define LESS_STATE_PAST_EOF  (1<<5)
#define LESS_STATE_MATCH_BACKWARDS (1<<6)

#if ENABLE_FEATURE_LESS_MARKS
static int mark_lines[15][2];
static int num_marks;
#endif

#if ENABLE_FEATURE_LESS_REGEXP
static int match_found;
static int *match_lines;
static int match_pos;
static int num_matches;
static regex_t old_pattern;
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
	/* Set terminal input to raw mode  (taken from vi.c) */
	tcsetattr(fileno(inp), TCSANOW, &term_vi);

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
		else if ((i = input - REAL_PAGE_UP) < 4)
			return 24 + i;
		else
			return 0; /* ?? */
	}
	/* The input is a normal ASCII value */
	set_tty_cooked();
	return input;
}

/* Move the cursor to a position (x,y), where (0,0) is the
   top-left corner of the console */
static void move_cursor(int x, int y)
{
	printf("\033[%i;%iH", x, y);
}

static void clear_line(void)
{
	move_cursor(height, 0);
	printf("\033[K");
}

static void data_readlines(void)
{
	unsigned i;
	unsigned n = 1;
	int w = width;
	char *last_nl = (char*)1; /* "not NULL" */
	char *current_line;
	FILE *fp;

	fp = filename ? xfopen(filename, "r") : stdin;
	flines = NULL;
	if (option_mask32 & FLAG_N) {
		w -= 6;
		if (w < 1) w = 1; /* paranoia */
	}
	for (i = 0; !feof(fp) && i <= MAXLINES; i++) {
		flines = xrealloc(flines, (i+1) * sizeof(char *));

		current_line = xmalloc(w);
 again:
		current_line[0] = '\0';
		fgets(current_line, w, fp);
		if (fp != stdin)
			die_if_ferror(fp, filename);

		/* Corner case: linewrap with only '\n' wrapping */
		/* Looks ugly on screen, so we handle it specially */
		if (!last_nl && current_line[0] == '\n') {
			last_nl = (char*)1; /* "not NULL" */
			n++;
			goto again;
		}
		last_nl = last_char_is(current_line, '\n');
		if (last_nl)
			*last_nl = '\0';
		if (option_mask32 & FLAG_N) {
			flines[i] = xasprintf((n <= 99999) ? "%5u %s" : "%05u %s",
						n % 100000, current_line);
			free(current_line);
			if (last_nl)
				n++;
		} else {
			flines[i] = xrealloc(current_line, strlen(current_line)+1);
		}
	}
	num_flines = i - 2;

	/* Reset variables for a new file */

	line_pos = 0;
	option_mask32 &= ~LESS_STATE_PAST_EOF;

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

	if (!(option_mask32 & LESS_STATE_PAST_EOF)) {
		if (!line_pos) {
			if (num_files > 1) {
				printf("%s%s %s%i%s%i%s%i-%i/%i ", HIGHLIGHT,
					filename, "(file ", current_file, " of ", num_files, ") lines ",
					line_pos + 1, line_pos + height - 1, num_flines + 1);
			} else {
				printf("%s%s lines %i-%i/%i ", HIGHLIGHT,
					filename, line_pos + 1, line_pos + height - 1,
					num_flines + 1);
			}
		} else {
			printf("%s %s lines %i-%i/%i ", HIGHLIGHT, filename,
				line_pos + 1, line_pos + height - 1, num_flines + 1);
		}

		if (line_pos == num_flines - height + 2) {
			printf("(END) %s", NORMAL);
			if ((num_files > 1) && (current_file != num_files))
				printf("%s- Next: %s%s", HIGHLIGHT, files[current_file], NORMAL);
		} else {
			percentage = calc_percent();
			printf("%i%% %s", percentage, NORMAL);
		}
	} else {
		printf("%s%s lines %i-%i/%i (END) ", HIGHLIGHT, filename,
				line_pos + 1, num_flines + 1, num_flines + 1);
		if ((num_files > 1) && (current_file != num_files))
			printf("- Next: %s", files[current_file]);
		printf("%s", NORMAL);
	}
}

/* Print a status line if -m was specified */
static void medium_status_print(void)
{
	int percentage;
	percentage = calc_percent();

	if (!line_pos)
		printf("%s%s %i%%%s", HIGHLIGHT, filename, percentage, NORMAL);
	else if (line_pos == num_flines - height + 2)
		printf("%s(END)%s", HIGHLIGHT, NORMAL);
	else
		printf("%s%i%%%s", HIGHLIGHT, percentage, NORMAL);
}
#endif

/* Print the status line */
static void status_print(void)
{
	/* Change the status if flags have been set */
#if ENABLE_FEATURE_LESS_FLAGS
	if (option_mask32 & FLAG_M)
		m_status_print();
	else if (option_mask32 & FLAG_m)
		medium_status_print();
	/* No flags set */
	else {
#endif
		if (!line_pos) {
			printf("%s%s %s", HIGHLIGHT, filename, NORMAL);
			if (num_files > 1)
				printf("%s%s%i%s%i%s%s", HIGHLIGHT, "(file ",
					current_file, " of ", num_files, ")", NORMAL);
		} else if (line_pos == num_flines - height + 2) {
			printf("%s%s %s", HIGHLIGHT, "(END)", NORMAL);
			if ((num_files > 1) && (current_file != num_files))
				printf("%s%s%s%s", HIGHLIGHT, "- Next: ", files[current_file], NORMAL);
		} else {
			putchar(':');
		}
#if ENABLE_FEATURE_LESS_FLAGS
	}
#endif
}

/* Print the buffer */
static void buffer_print(void)
{
	int i;

	printf("%s", CLEAR);
	if (num_flines >= height - 2) {
		for (i = 0; i < height - 1; i++)
			printf("%.*s\n", width, buffer[i]);
	} else {
		for (i = 1; i < (height - 1 - num_flines); i++)
			putchar('\n');
		for (i = 0; i < height - 1; i++)
			printf("%.*s\n", width, buffer[i]);
	}

	status_print();
}

/* Initialise the buffer */
static void buffer_init(void)
{
	int i;

	if (buffer == NULL) {
		/* malloc the number of lines needed for the buffer */
		buffer = xmalloc(height * sizeof(char *));
	}

	/* Fill the buffer until the end of the file or the
	   end of the buffer is reached */
	for (i = 0; (i < (height - 1)) && (i <= num_flines); i++) {
		buffer[i] = flines[i];
	}

	/* If the buffer still isn't full, fill it with blank lines */
	for (; i < (height - 1); i++) {
		buffer[i] = "";
	}
}

/* Move the buffer up and down in the file in order to scroll */
static void buffer_down(int nlines)
{
	int i;

	if (!(option_mask32 & LESS_STATE_PAST_EOF)) {
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
}

static void buffer_up(int nlines)
{
	int i;
	int tilde_line;

	if (!(option_mask32 & LESS_STATE_PAST_EOF)) {
		if (line_pos - nlines >= 0) {
			line_pos -= nlines;
			for (i = 0; i < (height - 1); i++) {
				buffer[i] = flines[line_pos + i];
			}
		} else {
		/* As the requested number of lines to move was too large, we
		   move one line up at a time until we can't. */
			while (line_pos != 0) {
				line_pos -= 1;
				for (i = 0; i < (height - 1); i++) {
					buffer[i] = flines[line_pos + i];
				}
			}
		}
	}
	else {
		/* Work out where the tildes start */
		tilde_line = num_flines - line_pos + 3;

		line_pos -= nlines;
		/* Going backwards nlines lines has taken us to a point where
		   nothing is past the EOF, so we revert to normal. */
		if (line_pos < num_flines - height + 3) {
			option_mask32 &= ~LESS_STATE_PAST_EOF;
			buffer_up(nlines);
		} else {
			/* We only move part of the buffer, as the rest
			is past the EOF */
			for (i = 0; i < (height - 1); i++) {
				if (i < tilde_line - nlines + 1) {
					buffer[i] = flines[line_pos + i];
				} else {
					if (line_pos >= num_flines - height + 2)
						buffer[i] = "~";
				}
			}
		}
	}
}

static void buffer_line(int linenum)
{
	int i;
	option_mask32 &= ~LESS_STATE_PAST_EOF;

	if (linenum < 0 || linenum > num_flines) {
		clear_line();
		printf("%s%s%i%s", HIGHLIGHT, "Cannot seek to line number ", linenum + 1, NORMAL);
	}
	else if (linenum < (num_flines - height - 2)) {
		for (i = 0; i < (height - 1); i++) {
			buffer[i] = flines[linenum + i];
		}
		line_pos = linenum;
		buffer_print();
	} else {
		for (i = 0; i < (height - 1); i++) {
			if (linenum + i < num_flines + 2)
				buffer[i] = flines[linenum + i];
			else
				buffer[i] = (option_mask32 & FLAG_TILDE) ? "" : "~";
		}
		line_pos = linenum;
		/* Set past_eof so buffer_down and buffer_up act differently */
		option_mask32 |= LESS_STATE_PAST_EOF;
		buffer_print();
	}
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
	clear_line();
	printf("Examine: ");
	free(filename);
	filename = xmalloc_getline(inp);
	files[num_files] = filename;
	current_file = num_files + 1;
	num_files++;
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
		clear_line();
		printf("%s%s%s", HIGHLIGHT, (direction > 0) ? "No next file" : "No previous file", NORMAL);
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
	clear_line();
	printf(" :");

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
			clear_line();
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

#if ENABLE_FEATURE_LESS_REGEXP
/* The below two regular expression handler functions NEED development. */

/* Get a regular expression from the user, and then go through the current
   file line by line, running a processing regex function on each one. */

static char *process_regex_on_line(char *line, regex_t *pattern, int action)
{
/* UNTESTED. LOOKED BUGGY AND LEAKY AS HELL. */
/* FIXED. NEED TESTING. */
	/* 'line' should be either returned or free()ed */

	/* This function takes the regex and applies it to the line.
	   Each part of the line that matches has the HIGHLIGHT
	   and NORMAL escape sequences placed around it by
	   insert_highlights if action = 1, or has the escape sequences
	   removed if action = 0, and then the line is returned. */
	int match_status;
	char *line2 = line;
	char *growline = xstrdup("");
	char *ng;
	regmatch_t match_structs;

	match_found = 0;
	match_status = regexec(pattern, line2, 1, &match_structs, 0);

	while (match_status == 0) {
		match_found = 1;
		if (action) {
			ng = xasprintf("%s%.*s%s%.*s%s", growline,
				match_structs.rm_so, line2, HIGHLIGHT,
				match_structs.rm_eo - match_structs.rm_so,
				line2 + match_structs.rm_so, NORMAL);
		} else {
			ng = xasprintf("%s%.*s%.*s", growline,
				match_structs.rm_so - 4, line2,
				match_structs.rm_eo - match_structs.rm_so,
				line2 + match_structs.rm_so);
		}
		free(growline); growline = ng;
		line2 += match_structs.rm_eo;
		match_status = regexec(pattern, line2, 1, &match_structs, REG_NOTBOL);
	}

	if (match_found) {
		ng = xasprintf("%s%s", growline, line2);
		free(line);
	} else {
		ng = line;
	}
	free(growline);
	return ng;
}

static void goto_match(int match)
{
	/* This goes to a specific match - all line positions of matches are
	   stored within the match_lines[] array. */
	if ((match < num_matches) && (match >= 0)) {
		buffer_line(match_lines[match]);
		match_pos = match;
	}
}

static void regex_process(void)
{
	char *uncomp_regex;
	int i;
	int j = 0;
	regex_t pattern;

	/* Get the uncompiled regular expression from the user */
	clear_line();
	putchar((option_mask32 & LESS_STATE_MATCH_BACKWARDS) ? '?' : '/');
	uncomp_regex = xmalloc_getline(inp);
	if (!uncomp_regex || !uncomp_regex[0]) {
		free(uncomp_regex);
		if (num_matches)
			goto_match((option_mask32 & LESS_STATE_MATCH_BACKWARDS)
						? match_pos - 1 : match_pos + 1);
		else
			buffer_print();
		return;
	}

	/* Compile the regex and check for errors */
	xregcomp(&pattern, uncomp_regex, 0);
	free(uncomp_regex);

	if (num_matches) {
		/* Get rid of all the highlights we added previously */
		for (i = 0; i <= num_flines; i++) {
			flines[i] = process_regex_on_line(flines[i], &old_pattern, 0);
		}
	}
	old_pattern = pattern;

	/* Reset variables */
	match_lines = xrealloc(match_lines, sizeof(int));
	match_lines[0] = -1;
	match_pos = 0;
	num_matches = 0;
	match_found = 0;
	/* Run the regex on each line of the current file here */
	for (i = 0; i <= num_flines; i++) {
		flines[i] = process_regex_on_line(flines[i], &pattern, 1);
		if (match_found) {
			match_lines = xrealloc(match_lines, (j + 1) * sizeof(int));
			match_lines[j] = i;
			j++;
		}
	}

	num_matches = j;
	if ((match_lines[0] != -1) && (num_flines > height - 2)) {
		if (option_mask32 & LESS_STATE_MATCH_BACKWARDS) {
			for (i = 0; i < num_matches; i++) {
				if (match_lines[i] > line_pos) {
					match_pos = i - 1;
					buffer_line(match_lines[match_pos]);
					break;
				}
			}
		} else
			buffer_line(match_lines[0]);
	} else
		buffer_init();
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
	printf("%s%s%i%s", HIGHLIGHT, "The status of the flag is: ", flag_val != 0, NORMAL);
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

	clear_line();
	printf("Log file: ");
	current_line = xmalloc_getline(inp);
	if (strlen(current_line) > 0) {
		fp = fopen(current_line, "w");
		free(current_line);
		if (!fp) {
			printf("%s%s%s", HIGHLIGHT, "Error opening log file", NORMAL);
			return;
		}
		for (i = 0; i < num_flines; i++)
			fprintf(fp, "%s\n", flines[i]);
		fclose(fp);
		buffer_print();
		return;
	}
	free(current_line);
	printf("%s%s%s", HIGHLIGHT, "No log file", NORMAL);
}

#if ENABLE_FEATURE_LESS_MARKS
static void add_mark(void)
{
	int letter;

	clear_line();
	printf("Mark: ");
	letter = tless_getch();

	if (isalpha(letter)) {

		/* If we exceed 15 marks, start overwriting previous ones */
		if (num_marks == 14)
			num_marks = 0;

		mark_lines[num_marks][0] = letter;
		mark_lines[num_marks][1] = line_pos;
		num_marks++;
	} else {
		clear_line();
		printf("%s%s%s", HIGHLIGHT, "Invalid mark letter", NORMAL);
	}
}

static void goto_mark(void)
{
	int letter;
	int i;

	clear_line();
	printf("Go to mark: ");
	letter = tless_getch();
	clear_line();

	if (isalpha(letter)) {
		for (i = 0; i <= num_marks; i++)
			if (letter == mark_lines[i][0]) {
				buffer_line(mark_lines[i][1]);
				break;
			}
		if ((num_marks == 14) && (letter != mark_lines[14][0]))
			printf("%s%s%s", HIGHLIGHT, "Mark not set", NORMAL);
	} else
		printf("%s%s%s", HIGHLIGHT, "Invalid mark letter", NORMAL);
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

	clear_line();

	if (strchr(flines[line_pos], bracket) == NULL)
		printf("%s%s%s", HIGHLIGHT, "No bracket in top line", NORMAL);
	else {
		for (i = line_pos + 1; i < num_flines; i++) {
			if (strchr(flines[i], opp_bracket(bracket)) != NULL) {
				bracket_line = i;
				break;
			}
		}

		if (bracket_line == -1)
			printf("%s%s%s", HIGHLIGHT, "No matching bracket found", NORMAL);

		buffer_line(bracket_line - height + 2);
	}
}

static void match_left_bracket(char bracket)
{
	int bracket_line = -1;
	int i;

	clear_line();

	if (strchr(flines[line_pos + height - 2], bracket) == NULL) {
		printf("%s%s%s", HIGHLIGHT, "No bracket in bottom line", NORMAL);
		printf("%s", flines[line_pos + height]);
		sleep(4);
	} else {
		for (i = line_pos + height - 2; i >= 0; i--) {
			if (strchr(flines[i], opp_bracket(bracket)) != NULL) {
				bracket_line = i;
				break;
			}
		}

		if (bracket_line == -1)
			printf("%s%s%s", HIGHLIGHT, "No matching bracket found", NORMAL);

		buffer_line(bracket_line);
	}
}

#endif  /* FEATURE_LESS_BRACKETS */

static void keypress_process(int keypress)
{
	switch (keypress) {
		case KEY_DOWN: case 'e': case 'j': case '\015':
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
			clear_line();
			m_status_print();
			break;
#endif
#if ENABLE_FEATURE_LESS_REGEXP
		case '/':
			option_mask32 &= ~LESS_STATE_MATCH_BACKWARDS;
			regex_process();
			break;
		case 'n':
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

	if (!num_files) {
		if (isatty(STDIN_FILENO)) {
			/* Just "less"? No file and no redirection? */
			bb_error_msg("missing filename");
			bb_show_usage();
		}
	} else
		filename = xstrdup(files[0]);

	/* FIXME: another popular pager, most, detects when stdout
	 * is not a tty and turns into cat */
	inp = xfopen(CURRENT_TTY, "r");

	get_terminal_width_height(fileno(inp), &width, &height);
	data_readlines();

	signal(SIGTERM, sig_catcher);
	signal(SIGINT, sig_catcher);
	tcgetattr(fileno(inp), &term_orig);

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
