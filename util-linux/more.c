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
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "busybox.h"


#ifdef CONFIG_FEATURE_USE_TERMIOS
static int cin_fileno;
#include <termios.h>
#define setTermSettings(fd,argp) tcsetattr(fd,TCSANOW,argp)
#define getTermSettings(fd,argp) tcgetattr(fd, argp);

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
#endif /* CONFIG_FEATURE_USE_TERMIOS */


extern int more_main(int argc, char **argv)
{
	int c, lines, input = 0;
	int please_display_more_prompt = 0;
	struct stat st;
	FILE *file;
	FILE *cin;
	int len, page_height;
	int terminal_width;
	int terminal_height;
#ifndef CONFIG_FEATURE_USE_TERMIOS
	int cin_fileno;
#endif

	argc--;
	argv++;


	/* not use inputing from terminal if usage: more > outfile */
	if(isatty(STDOUT_FILENO)) {
		cin = fopen(CURRENT_TTY, "r");
		if (!cin)
			cin = bb_xfopen(CONSOLE_DEV, "r");
		cin_fileno = fileno(cin);
		please_display_more_prompt = 2;
#ifdef CONFIG_FEATURE_USE_TERMIOS
		getTermSettings(cin_fileno, &initial_settings);
		new_settings = initial_settings;
		new_settings.c_lflag &= ~ICANON;
		new_settings.c_lflag &= ~ECHO;
		new_settings.c_cc[VMIN] = 1;
		new_settings.c_cc[VTIME] = 0;
		setTermSettings(cin_fileno, &new_settings);
		atexit(set_tty_to_initial_mode);
		(void) signal(SIGINT, gotsig);
		(void) signal(SIGQUIT, gotsig);
		(void) signal(SIGTERM, gotsig);
#endif
	} else {
		cin = stdin;
	}

	do {
		if (argc == 0) {
			file = stdin;
		} else
			file = bb_wfopen(*argv, "r");
		if(file==0)
			goto loop;

		st.st_size = 0;
		fstat(fileno(file), &st);

		please_display_more_prompt &= ~1;

		get_terminal_width_height(cin_fileno, &terminal_width, &terminal_height);
		if (terminal_height > 4)
			terminal_height -= 2;
		if (terminal_width > 0)
			terminal_width -= 1;

		len=0;
		lines = 0;
		page_height = terminal_height;
		while ((c = getc(file)) != EOF) {

			if ((please_display_more_prompt & 3) == 3) {
				len = printf("--More-- ");
				if (file != stdin && st.st_size > 0) {
#if _FILE_OFFSET_BITS == 64
					len += printf("(%d%% of %lld bytes)",
						   (int) (100 * ((double) ftell(file) /
						   (double) st.st_size)), (long long)st.st_size);
#else
					len += printf("(%d%% of %ld bytes)",
						   (int) (100 * ((double) ftell(file) /
						   (double) st.st_size)), (long)st.st_size);
#endif
				}

				fflush(stdout);

				/*
				 * We've just displayed the "--More--" prompt, so now we need
				 * to get input from the user.
				 */
				input = getc(cin);
#ifndef CONFIG_FEATURE_USE_TERMIOS
				printf("\033[A"); /* up cursor */
#endif
				/* Erase the "More" message */
				putc('\r', stdout);
				while (--len >= 0)
					putc(' ', stdout);
				putc('\r', stdout);
				len=0;
				lines = 0;
				page_height = terminal_height;
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
					if (quot) {
						if (rem)
							page_height-=quot;
						else
							page_height-=(quot-1);
					}
				}
				if (++lines >= page_height) {
					please_display_more_prompt |= 1;
				}
				len=0;
			}
			/*
			 * If we just read a newline from the file being 'mored' and any
			 * key other than a return is hit, scroll by one page
			 */
			putc(c, stdout);
			len++;
		}
		fclose(file);
		fflush(stdout);
loop:
		argv++;
	} while (--argc > 0);
  end:
	return 0;
}
