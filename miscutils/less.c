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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <ctype.h>

#include "busybox.h"

#ifdef CONFIG_FEATURE_LESS_REGEXP
#include "xregex.h"
#endif


/* These are the escape sequences corresponding to special keys */
#define REAL_KEY_UP 'A'
#define REAL_KEY_DOWN 'B'
#define REAL_KEY_RIGHT 'C'
#define REAL_KEY_LEFT 'D'
#define REAL_PAGE_UP '5'
#define REAL_PAGE_DOWN '6'

/* These are the special codes assigned by this program to the special keys */
#define PAGE_UP 20
#define PAGE_DOWN 21
#define KEY_UP 22
#define KEY_DOWN 23
#define KEY_RIGHT 24
#define KEY_LEFT 25

/* The escape codes for highlighted and normal text */
#define HIGHLIGHT "\033[7m"
#define NORMAL "\033[0m"

/* The escape code to clear the screen */
#define CLEAR "\033[H\033[J"

/* Maximum number of lines in a file */
#define MAXLINES 10000

/* Get height and width of terminal */
#define tty_width_height()              get_terminal_width_height(0, &width, &height)

static int height;
static int width;
static char **files;
static char filename[256];
static char **buffer;
static char **flines;
static int current_file = 1;
static int line_pos;
static int num_flines;
static int num_files = 1;
static int past_eof;

/* Command line options */
static unsigned long flags;
#define FLAG_E 1
#define FLAG_M (1<<1)
#define FLAG_m (1<<2)
#define FLAG_N (1<<3)
#define FLAG_TILDE (1<<4)

/* This is needed so that program behaviour changes when input comes from
   stdin */
static int inp_stdin;

#ifdef CONFIG_FEATURE_LESS_MARKS
static int mark_lines[15][2];
static int num_marks;
#endif

#ifdef CONFIG_FEATURE_LESS_REGEXP
static int match_found;
static int *match_lines;
static int match_pos;
static int num_matches;
static int match_backwards;
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

/* Set terminal input to raw mode  (taken from vi.c) */
static void set_tty_raw(void)
{
	tcsetattr(fileno(inp), TCSANOW, &term_vi);
}

/* Exit the program gracefully */
static void tless_exit(int code)
{
	/* TODO: We really should save the terminal state when we start,
		 and restore it when we exit. Less does this with the
		 "ti" and "te" termcap commands; can this be done with
		 only termios.h? */

	putchar('\n');
	exit(code);
}

/* Grab a character from input without requiring the return key. If the
   character is ASCII \033, get more characters and assign certain sequences
   special return codes. Note that this function works best with raw input. */
static int tless_getch(void)
{
	int input;

	set_tty_raw();

	input = getc(inp);
	/* Detect escape sequences (i.e. arrow keys) and handle
	   them accordingly */

	if (input == '\033' && getc(inp) == '[') {
		input = getc(inp);
		set_tty_cooked();
		if (input == REAL_KEY_UP)
			return KEY_UP;
		else if (input == REAL_KEY_DOWN)
			return KEY_DOWN;
		else if (input == REAL_KEY_RIGHT)
			return KEY_RIGHT;
		else if (input == REAL_KEY_LEFT)
			return KEY_LEFT;
		else if (input == REAL_PAGE_UP)
			return PAGE_UP;
		else if (input == REAL_PAGE_DOWN)
			return PAGE_DOWN;
	}
	/* The input is a normal ASCII value */
	else {
		set_tty_cooked();
		return input;
	}
	return 0;
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

/* This adds line numbers to every line, as the -N flag necessitates */
static void add_linenumbers(void)
{
	char current_line[256];
	int i;

	for (i = 0; i <= num_flines; i++) {
		safe_strncpy(current_line, flines[i], 256);
		flines[i] = bb_xasprintf("%5d %s", i + 1, current_line);
	}
}

static void data_readlines(void)
{
	int i;
	char current_line[256];
	FILE *fp;

	fp = (inp_stdin) ? stdin : bb_xfopen(filename, "rt");
	flines = NULL;
	for (i = 0; (feof(fp)==0) && (i <= MAXLINES); i++) {
		strcpy(current_line, "");
		fgets(current_line, 256, fp);
		if (fp != stdin)
			bb_xferror(fp, filename);
		flines = xrealloc(flines, (i+1) * sizeof(char *));
		flines[i] = bb_xstrdup(current_line);
	}
	num_flines = i - 2;

	/* Reset variables for a new file */

	line_pos = 0;
	past_eof = 0;

	fclose(fp);

	if (inp == NULL)
		inp = (inp_stdin) ? bb_xfopen(CURRENT_TTY, "r") : stdin;

	if (flags & FLAG_N)
		add_linenumbers();
}

#ifdef CONFIG_FEATURE_LESS_FLAGS

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

	if (!past_eof) {
		if (!line_pos) {
			if (num_files > 1)
				printf("%s%s %s%i%s%i%s%i-%i/%i ", HIGHLIGHT, filename, "(file ", current_file, " of ", num_files, ") lines ", line_pos + 1, line_pos + height - 1, num_flines + 1);
			else {
				printf("%s%s lines %i-%i/%i ", HIGHLIGHT, filename, line_pos + 1, line_pos + height - 1, num_flines + 1);
			}
		}
		else {
			printf("%s %s lines %i-%i/%i ", HIGHLIGHT, filename, line_pos + 1, line_pos + height - 1, num_flines + 1);
		}

		if (line_pos == num_flines - height + 2) {
			printf("(END) %s", NORMAL);
			if ((num_files > 1) && (current_file != num_files))
				printf("%s- Next: %s%s", HIGHLIGHT, files[current_file], NORMAL);
		}
		else {
			percentage = calc_percent();
			printf("%i%% %s", percentage, NORMAL);
		}
	}
	else {
		printf("%s%s lines %i-%i/%i (END) ", HIGHLIGHT, filename, line_pos + 1, num_flines + 1, num_flines + 1);
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
#ifdef CONFIG_FEATURE_LESS_FLAGS
	if (flags & FLAG_M)
		m_status_print();
	else if (flags & FLAG_m)
		medium_status_print();
	/* No flags set */
	else {
#endif
		if (!line_pos) {
			printf("%s%s %s", HIGHLIGHT, filename, NORMAL);
			if (num_files > 1)
				printf("%s%s%i%s%i%s%s", HIGHLIGHT, "(file ", current_file, " of ", num_files, ")", NORMAL);
		}
		else if (line_pos == num_flines - height + 2) {
			printf("%s%s %s", HIGHLIGHT, "(END)", NORMAL);
			if ((num_files > 1) && (current_file != num_files))
				printf("%s%s%s%s", HIGHLIGHT, "- Next: ", files[current_file], NORMAL);
		}
		else {
			putchar(':');
		}
#ifdef CONFIG_FEATURE_LESS_FLAGS
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
			printf("%s", buffer[i]);
	}
	else {
		for (i = 1; i < (height - 1 - num_flines); i++)
			putchar('\n');
		for (i = 0; i < height - 1; i++)
			printf("%s", buffer[i]);
	}

	status_print();
}

/* Initialise the buffer */
static void buffer_init(void)
{
	int i;

	if (buffer == NULL) {
		/* malloc the number of lines needed for the buffer */
		buffer = xrealloc(buffer, height * sizeof(char *));
	} else {
		for (i = 0; i < (height - 1); i++)
			free(buffer[i]);
	}

	/* Fill the buffer until the end of the file or the
	   end of the buffer is reached */
	for (i = 0; (i < (height - 1)) && (i <= num_flines); i++) {
		buffer[i] = bb_xstrdup(flines[i]);
	}

	/* If the buffer still isn't full, fill it with blank lines */
	for (; i < (height - 1); i++) {
		buffer[i] = bb_xstrdup("");
	}
}

/* Move the buffer up and down in the file in order to scroll */
static void buffer_down(int nlines)
{
	int i;

	if (!past_eof) {
		if (line_pos + (height - 3) + nlines < num_flines) {
			line_pos += nlines;
			for (i = 0; i < (height - 1); i++) {
				free(buffer[i]);
				buffer[i] = bb_xstrdup(flines[line_pos + i]);
			}
		}
		else {
			/* As the number of lines requested was too large, we just move
			to the end of the file */
			while (line_pos + (height - 3) + 1 < num_flines) {
				line_pos += 1;
				for (i = 0; i < (height - 1); i++) {
					free(buffer[i]);
					buffer[i] = bb_xstrdup(flines[line_pos + i]);
				}
			}
		}

		/* We exit if the -E flag has been set */
		if ((flags & FLAG_E) && (line_pos + (height - 2) == num_flines))
			tless_exit(0);
	}
}

static void buffer_up(int nlines)
{
	int i;
	int tilde_line;

	if (!past_eof) {
		if (line_pos - nlines >= 0) {
			line_pos -= nlines;
			for (i = 0; i < (height - 1); i++) {
				free(buffer[i]);
				buffer[i] = bb_xstrdup(flines[line_pos + i]);
			}
		}
		else {
		/* As the requested number of lines to move was too large, we
		   move one line up at a time until we can't. */
			while (line_pos != 0) {
				line_pos -= 1;
				for (i = 0; i < (height - 1); i++) {
					free(buffer[i]);
					buffer[i] = bb_xstrdup(flines[line_pos + i]);
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
			past_eof = 0;
			buffer_up(nlines);
		}
		else {
			/* We only move part of the buffer, as the rest
			is past the EOF */
			for (i = 0; i < (height - 1); i++) {
				free(buffer[i]);
				if (i < tilde_line - nlines + 1)
					buffer[i] = bb_xstrdup(flines[line_pos + i]);
				else {
					if (line_pos >= num_flines - height + 2)
						buffer[i] = bb_xstrdup("~\n");
				}
			}
		}
	}
}

static void buffer_line(int linenum)
{
	int i;
	past_eof = 0;

	if (linenum < 0 || linenum > num_flines) {
		clear_line();
		printf("%s%s%i%s", HIGHLIGHT, "Cannot seek to line number ", linenum + 1, NORMAL);
	}
	else if (linenum < (num_flines - height - 2)) {
		for (i = 0; i < (height - 1); i++) {
			free(buffer[i]);
			buffer[i] = bb_xstrdup(flines[linenum + i]);
		}
		line_pos = linenum;
		buffer_print();
	}
	else {
		for (i = 0; i < (height - 1); i++) {
			free(buffer[i]);
			if (linenum + i < num_flines + 2)
				buffer[i] = bb_xstrdup(flines[linenum + i]);
			else
				buffer[i] = bb_xstrdup((flags & FLAG_TILDE) ? "\n" : "~\n");
		}
		line_pos = linenum;
		/* Set past_eof so buffer_down and buffer_up act differently */
		past_eof = 1;
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
	int newline_offset;

	clear_line();
	printf("Examine: ");
	fgets(filename, 256, inp);

	/* As fgets adds a newline to the end of an input string, we
	   need to remove it */
	newline_offset = strlen(filename) - 1;
	filename[newline_offset] = '\0';

	files[num_files] = bb_xstrdup(filename);
	current_file = num_files + 1;
	num_files++;

	inp_stdin = 0;
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
		strcpy(filename, files[current_file - 1]);
		reinitialise();
	}
	else {
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
	}
	else {
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
#ifdef CONFIG_FEATURE_LESS_FLAGS
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

#ifdef CONFIG_FEATURE_LESS_REGEXP
/* The below two regular expression handler functions NEED development. */

/* Get a regular expression from the user, and then go through the current
   file line by line, running a processing regex function on each one. */

static char *process_regex_on_line(char *line, regex_t *pattern, int action)
{
	/* This function takes the regex and applies it to the line.
	   Each part of the line that matches has the HIGHLIGHT
	   and NORMAL escape sequences placed around it by
	   insert_highlights if action = 1, or has the escape sequences
	   removed if action = 0, and then the line is returned. */
	int match_status;
	char *line2 = (char *) xmalloc((sizeof(char) * (strlen(line) + 1)) + 64);
	char *growline = "";
	regmatch_t match_structs;

	line2 = bb_xstrdup(line);

	match_found = 0;
	match_status = regexec(pattern, line2, 1, &match_structs, 0);
	
	while (match_status == 0) {
		if (match_found == 0)
			match_found = 1;
		
		if (action) {
			growline = bb_xasprintf("%s%.*s%s%.*s%s", growline, match_structs.rm_so, line2, HIGHLIGHT, match_structs.rm_eo - match_structs.rm_so, line2 + match_structs.rm_so, NORMAL); 
		}
		else {
			growline = bb_xasprintf("%s%.*s%.*s", growline, match_structs.rm_so - 4, line2, match_structs.rm_eo - match_structs.rm_so, line2 + match_structs.rm_so);
		}
		
		line2 += match_structs.rm_eo;
		match_status = regexec(pattern, line2, 1, &match_structs, REG_NOTBOL);
	}
	
	growline = bb_xasprintf("%s%s", growline, line2);
	
	return (match_found ? growline : line);
	
	free(growline);
	free(line2);
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
	char uncomp_regex[100];
	char *current_line;
	int i;
	int j = 0;
	regex_t pattern;
	/* Get the uncompiled regular expression from the user */
	clear_line();
	putchar((match_backwards) ? '?' : '/');
	uncomp_regex[0] = 0;
	fgets(uncomp_regex, sizeof(uncomp_regex), inp);
	
	if (strlen(uncomp_regex) == 1) {
		if (num_matches)
			goto_match(match_backwards ? match_pos - 1 : match_pos + 1);
		else
			buffer_print();
		return;
	}
	uncomp_regex[strlen(uncomp_regex) - 1] = '\0';
	
	/* Compile the regex and check for errors */
	xregcomp(&pattern, uncomp_regex, 0);

	if (num_matches) {
		/* Get rid of all the highlights we added previously */
		for (i = 0; i <= num_flines; i++) {
			current_line = process_regex_on_line(flines[i], &old_pattern, 0);
			flines[i] = bb_xstrdup(current_line);
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
		current_line = process_regex_on_line(flines[i], &pattern, 1);
		flines[i] = bb_xstrdup(current_line);
		if (match_found) {
			match_lines = xrealloc(match_lines, (j + 1) * sizeof(int));
			match_lines[j] = i;
			j++;
		}
	}
	
	num_matches = j;
	if ((match_lines[0] != -1) && (num_flines > height - 2)) {
		if (match_backwards) {
			for (i = 0; i < num_matches; i++) {
				if (match_lines[i] > line_pos) {
					match_pos = i - 1;
					buffer_line(match_lines[match_pos]);
					break;
				}
			}
		}
		else
			buffer_line(match_lines[0]);
	}
	else
		buffer_init();
}
#endif

static void number_process(int first_digit)
{
	int i = 1;
	int num;
	char num_input[80];
	char keypress;
	char *endptr;

	num_input[0] = first_digit;

	/* Clear the current line, print a prompt, and then print the digit */
	clear_line();
	printf(":%c", first_digit);

	/* Receive input until a letter is given (max 80 chars)*/
	while((i < 80) && (num_input[i] = tless_getch()) && isdigit(num_input[i])) {
		putchar(num_input[i]);
		i++;
	}

	/* Take the final letter out of the digits string */
	keypress = num_input[i];
	num_input[i] = '\0';
	num = strtol(num_input, &endptr, 10);
	if (endptr==num_input || *endptr!='\0' || num < 1 || num > MAXLINES) {
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
#ifdef CONFIG_FEATURE_LESS_REGEXP
		case 'n':
			goto_match(match_pos + num);
			break;
		case '/':
			match_backwards = 0;
			regex_process();
			break;
		case '?':
			match_backwards = 1;
			regex_process();
			break;
#endif
		default:
			break;
	}
}

#ifdef CONFIG_FEATURE_LESS_FLAGCS
static void flag_change(void)
{
	int keypress;

	clear_line();
	putchar('-');
	keypress = tless_getch();

	switch (keypress) {
		case 'M':
			flags ^= FLAG_M;
			break;
		case 'm':
			flags ^= FLAG_m;
			break;
		case 'E':
			flags ^= FLAG_E;
			break;
		case '~':
			flags ^= FLAG_TILDE;
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
			flag_val = flags & FLAG_M;
			break;
		case 'm':
			flag_val = flags & FLAG_m;
			break;
		case '~':
			flag_val = flags & FLAG_TILDE;
			break;
		case 'N':
			flag_val = flags & FLAG_N;
			break;
		case 'E':
			flag_val = flags & FLAG_E;
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
	char current_line[256];
	int i;
	FILE *fp;

	clear_line();
	printf("Log file: ");
	fgets(current_line, 256, inp);
	current_line[strlen(current_line) - 1] = '\0';
	if (strlen(current_line) > 1) {
		fp = bb_xfopen(current_line, "w");
		for (i = 0; i < num_flines; i++)
			fprintf(fp, "%s", flines[i]);
		fclose(fp);
		buffer_print();
	}
	else
		printf("%sNo log file%s", HIGHLIGHT, NORMAL);
}

#ifdef CONFIG_FEATURE_LESS_MARKS
static void add_mark(void)
{
	int letter;
	int mark_line;

	clear_line();
	printf("Mark: ");
	letter = tless_getch();

	if (isalpha(letter)) {
		mark_line = line_pos;

		/* If we exceed 15 marks, start overwriting previous ones */
		if (num_marks == 14)
			num_marks = 0;

		mark_lines[num_marks][0] = letter;
		mark_lines[num_marks][1] = line_pos;
		num_marks++;
	}
	else {
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
	}
	else
		printf("%s%s%s", HIGHLIGHT, "Invalid mark letter", NORMAL);
}
#endif


#ifdef CONFIG_FEATURE_LESS_BRACKETS

static char opp_bracket(char bracket)
{
	switch (bracket) {
		case '{': case '[':
			return bracket + 2;
			break;
		case '(':
			return ')';
			break;
		case '}': case ']':
			return bracket - 2;
			break;
		case ')':
			return '(';
			break;
		default:
			return 0;
			break;
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
	}
	else {
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

#endif  /* CONFIG_FEATURE_LESS_BRACKETS */

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
		case 'g': case 'p': case '<': case '%':
			buffer_line(0);
			break;
		case 'G': case '>':
			buffer_line(num_flines - height + 2);
			break;
		case 'q': case 'Q':
			tless_exit(0);
			break;
#ifdef CONFIG_FEATURE_LESS_MARKS
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
			if (inp_stdin)
				save_input_to_file();
			break;
		case 'E':
			examine_file();
			break;
#ifdef CONFIG_FEATURE_LESS_FLAGS
		case '=':
			clear_line();
			m_status_print();
			break;
#endif
#ifdef CONFIG_FEATURE_LESS_REGEXP
		case '/':
			match_backwards = 0;
			regex_process();
			break;
		case 'n':
			goto_match(match_pos + 1);
			break;
		case 'N':
			goto_match(match_pos - 1);
			break;
		case '?':
			match_backwards = 1;
			regex_process();
			break;
#endif
#ifdef CONFIG_FEATURE_LESS_FLAGCS
		case '-':
			flag_change();
			buffer_print();
			break;
		case '_':
			show_flag_status();
			break;
#endif
#ifdef CONFIG_FEATURE_LESS_BRACKETS
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

int less_main(int argc, char **argv) {

	int keypress;

	flags = bb_getopt_ulflags(argc, argv, "EMmN~");

	argc -= optind;
	argv += optind;
	files = argv;
	num_files = argc;

	if (!num_files) {
		if (ttyname(STDIN_FILENO) == NULL)
			inp_stdin = 1;
		else {
			bb_error_msg("Missing filename");
			bb_show_usage();
		}
	}

	strcpy(filename, (inp_stdin) ? bb_msg_standard_input : files[0]);
	tty_width_height();
	data_readlines();
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
