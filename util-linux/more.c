/* vi: set sw=4 ts=4: */
/*
 * Mini more implementation for busybox
 *
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 *
 * Latest version blended together by Erik Andersen <andersen@lineo.com>,
 * based on the original more implementation by Bruce, and code from the 
 * Debian boot-floppies team.
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
#include <sys/ioctl.h>
#include "busybox.h"
#define BB_DECLARE_EXTERN
#define bb_need_help
#include "messages.c"

/* ED: sparc termios is broken: revert back to old termio handling. */
#ifdef BB_FEATURE_USE_TERMIOS
#	if #cpu(sparc)
#		include <termio.h>
#		define termios termio
#		define setTermSettings(fd,argp) ioctl(fd,TCSETAF,argp)
#		define getTermSettings(fd,argp) ioctl(fd,TCGETA,argp)
#	else
#		include <termios.h>
#		define setTermSettings(fd,argp) tcsetattr(fd,TCSANOW,argp)
#		define getTermSettings(fd,argp) tcgetattr(fd, argp);
#	endif

static FILE *cin;

static struct termios initial_settings, new_settings;

static void gotsig(int sig)
{
	setTermSettings(fileno(cin), &initial_settings);
	putchar('\n');
	exit(EXIT_FAILURE);
}
#endif /* BB_FEATURE_USE_TERMIOS */


static int terminal_width = 79;	/* not 80 in case terminal has linefold bug */
static int terminal_height = 24;


extern int more_main(int argc, char **argv)
{
	int c, lines, input = 0;
	int please_display_more_prompt;
	struct stat st;
	FILE *file;
	int len, page_height;

#if defined BB_FEATURE_AUTOWIDTH && defined BB_FEATURE_USE_TERMIOS
	struct winsize win = { 0, 0, 0, 0 };
#endif

	argc--;
	argv++;

	do {
		if (argc == 0) {
			file = stdin;
		} else
			file = xfopen(*argv, "r");

		fstat(fileno(file), &st);

#ifdef BB_FEATURE_USE_TERMIOS
		cin = fopen("/dev/tty", "r");
		if (!cin)
			cin = xfopen("/dev/console", "r");
		getTermSettings(fileno(cin), &initial_settings);
		new_settings = initial_settings;
		new_settings.c_cc[VMIN] = 1;
		new_settings.c_cc[VTIME] = 0;
		new_settings.c_lflag &= ~ICANON;
		new_settings.c_lflag &= ~ECHO;
		setTermSettings(fileno(cin), &new_settings);

#	ifdef BB_FEATURE_AUTOWIDTH
		ioctl(fileno(stdout), TIOCGWINSZ, &win);
		if (win.ws_row > 4)
			terminal_height = win.ws_row - 2;
		if (win.ws_col > 0)
			terminal_width = win.ws_col - 1;
#	endif

		(void) signal(SIGINT, gotsig);
		(void) signal(SIGQUIT, gotsig);
		(void) signal(SIGTERM, gotsig);

#endif
		len=0;
		lines = 0;
		page_height = terminal_height;
		please_display_more_prompt = 0;
		while ((c = getc(file)) != EOF) {

			if (please_display_more_prompt) {
				len = printf("--More-- ");
				if (file != stdin) {
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
				len += printf("%s",
#ifdef BB_FEATURE_USE_TERMIOS
							   ""
#else
							   "\n"
#endif
					);

				fflush(stdout);

				/*
				 * We've just displayed the "--More--" prompt, so now we need
				 * to get input from the user.
				 */
#ifdef BB_FEATURE_USE_TERMIOS
				input = getc(cin);
#else
				input = getc(stdin);
#endif

#ifdef BB_FEATURE_USE_TERMIOS
				/* Erase the "More" message */
				putc('\r', stdout);
				while (--len >= 0)
					putc(' ', stdout);
				putc('\r', stdout);
				fflush(stdout);
#endif
				len=0;
				lines = 0;
				page_height = terminal_height;
				please_display_more_prompt = 0;
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
				switch (input) {
				case 'q':
					goto end;
				case '\n':
					/* increment by just one line if we are at 
					 * the end of this line*/
					please_display_more_prompt = 1;
					break;
				}
				/* Adjust the terminal height for any overlap, so that
				 * no lines get lost off the top. */
				if (len >= terminal_width) {
					div_t result = div( len, terminal_width); 
					if (result.quot) {
						if (result.rem)
							page_height-=result.quot;
						else
							page_height-=(result.quot-1);
					}
				}
				if (++lines >= page_height) {
					please_display_more_prompt = 1;
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

		argv++;
	} while (--argc > 0);
  end:
#ifdef BB_FEATURE_USE_TERMIOS
	gotsig(0);
#endif
	return(TRUE);
}
