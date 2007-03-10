/* vi: set sw=4 ts=4: */
/*
 * Mini more implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Latest version blended together by Erik Andersen <andersen@codepoet.org>,
 * based on the original more implementation by Bruce, and code from the
 * Debian boot-floppies team.
 *
 * Termios corrects by Vladimir Oleynik <dzo@simtreas.ru>
 *
 * Licensed under GPLv2 or later, see file License in this tarball for details.
 */

#include "busybox.h"


#if ENABLE_FEATURE_USE_TERMIOS
static int cin_fileno;
#include <termios.h>
#define setTermSettings(fd, argp) tcsetattr(fd, TCSANOW, argp)
#define getTermSettings(fd, argp) tcgetattr(fd, argp);

static struct termios initial_settings, new_settings;

static void set_tty_to_initial_mode(void)
{
	setTermSettings(cin_fileno, &initial_settings);
}

static void gotsig(int sig)
{
	putchar('\n');
	exit(EXIT_FAILURE);
}
#endif /* FEATURE_USE_TERMIOS */


int more_main(int argc, char **argv);
int more_main(int argc, char **argv)
{
	int c, lines, input = 0;
	int please_display_more_prompt = 0;
	struct stat st;
	FILE *file;
	FILE *cin;
	int len, page_height;
	int terminal_width;
	int terminal_height;

	argv++;
	/* Another popular pager, most, detects when stdout
	 * is not a tty and turns into cat. This makes sense. */
	if (!isatty(STDOUT_FILENO))
		return bb_cat(argv);
	cin = fopen(CURRENT_TTY, "r");
	if (!cin)
		return bb_cat(argv);

#if ENABLE_FEATURE_USE_TERMIOS
	cin_fileno = fileno(cin);
	getTermSettings(cin_fileno, &initial_settings);
	new_settings = initial_settings;
	new_settings.c_lflag &= ~ICANON;
	new_settings.c_lflag &= ~ECHO;
	new_settings.c_cc[VMIN] = 1;
	new_settings.c_cc[VTIME] = 0;
	setTermSettings(cin_fileno, &new_settings);
	atexit(set_tty_to_initial_mode);
	signal(SIGINT, gotsig);
	signal(SIGQUIT, gotsig);
	signal(SIGTERM, gotsig);
#endif
	please_display_more_prompt = 2;

	do {
		file = stdin;
		if (*argv) {
			file = fopen_or_warn(*argv, "r");
			if (!file)
				continue;
		}
		st.st_size = 0;
		fstat(fileno(file), &st);

		please_display_more_prompt &= ~1;
		/* never returns w, h <= 1 */
		get_terminal_width_height(fileno(cin), &terminal_width, &terminal_height);
		terminal_width -= 1;
		terminal_height -= 1;

		len = 0;
		lines = 0;
		page_height = terminal_height;
		while ((c = getc(file)) != EOF) {

			if ((please_display_more_prompt & 3) == 3) {
				len = printf("--More-- ");
				if (/*file != stdin &&*/ st.st_size > 0) {
					len += printf("(%d%% of %"OFF_FMT"d bytes)",
						(int) (ftello(file)*100 / st.st_size),
						st.st_size);
				}
				fflush(stdout);

				/*
				 * We've just displayed the "--More--" prompt, so now we need
				 * to get input from the user.
				 */
				input = getc(cin);
#if !ENABLE_FEATURE_USE_TERMIOS
				printf("\033[A"); /* up cursor */
#endif
				/* Erase the "More" message */
				printf("\r%*s\r", len, "");
				len = 0;
				lines = 0;
				/* Bottom line on page will become top line
				 * after one page forward. Thus -1: */
				page_height = terminal_height - 1;
				please_display_more_prompt &= ~1;

				if (input == 'q')
					goto end;
			}

			/*
			 * There are two input streams to worry about here:
			 *
			 * c     : the character we are reading from the file being "mored"
			 * input : a character received from the keyboard
			 *
			 * If we hit a newline in the _file_ stream, we want to test and
			 * see if any characters have been hit in the _input_ stream. This
			 * allows the user to quit while in the middle of a file.
			 */
			if (c == '\n') {
				/* increment by just one line if we are at
				 * the end of this line */
				if (input == '\n')
					please_display_more_prompt |= 1;
				/* Adjust the terminal height for any overlap, so that
				 * no lines get lost off the top. */
				if (len >= terminal_width) {
					int quot, rem;
					quot = len / terminal_width;
					rem  = len - (quot * terminal_width);
					page_height -= (quot - 1);
					if (rem)
						page_height--;
				}
				if (++lines >= page_height) {
					please_display_more_prompt |= 1;
				}
				len = 0;
			}
			/*
			 * If we just read a newline from the file being 'mored' and any
			 * key other than a return is hit, scroll by one page
			 */
			putc(c, stdout);
			/* My small mind cannot fathom tabs, backspaces,
			 * and UTF-8 */
			len++;
		}
		fclose(file);
		fflush(stdout);
	} while (*argv && *++argv);
 end:
	return 0;
}
