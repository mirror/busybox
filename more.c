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

#include "internal.h"
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>

static const char more_usage[] = "more [file ...]\n";

/* ED: sparc termios is broken: revert back to old termio handling. */
#ifdef BB_FEATURE_USE_TERMIOS

#if #cpu(sparc)
#      define USE_OLD_TERMIO
#      include <termio.h>
#      define termios termio
#      define stty(fd,argp) ioctl(fd,TCSETAF,argp)
#else
#      include <termios.h>
#      define stty(fd,argp) tcsetattr(fd,TCSANOW,argp)
#endif

    FILE *cin;
    struct termios initial_settings, new_settings;

    void gotsig(int sig) { 
	    stty(fileno(cin), &initial_settings);
	    fprintf(stdout, "\n");
	    exit( TRUE);
    }
#endif



#define TERMINAL_WIDTH	79	/* not 80 in case terminal has linefold bug */
#define TERMINAL_HEIGHT	24


#if defined BB_FEATURE_AUTOWIDTH
static int terminal_width = 0, terminal_height = 0;
#else
#define terminal_width	TERMINAL_WIDTH
#define terminal_height	TERMINAL_HEIGHT
#endif



extern int more_main(int argc, char **argv)
{
    int c, lines=0, input=0;
    int next_page=0;
    struct stat st;	
    FILE *file;
#ifdef BB_FEATURE_AUTOWIDTH
    struct winsize win = {0,0};
#endif

    argc--;
    argv++;

    if ( argc > 0 && (strcmp(*argv,"--help")==0 || strcmp(*argv,"-h")==0) ) {
	usage (more_usage);
    }
    do {
	if (argc==0) {
	    file = stdin;
	}
	else
	    file = fopen(*argv, "r");

	if (file == NULL) {
	    perror(*argv);
	    exit(FALSE);
	}
	fstat(fileno(file), &st);

#ifdef BB_FEATURE_USE_TERMIOS
	cin = fopen("/dev/tty", "r");
	if (!cin)
	    cin = fopen("/dev/console", "r");
#ifdef USE_OLD_TERMIO
	ioctl(fileno(cin),TCGETA,&initial_settings);
#else
	tcgetattr(fileno(cin),&initial_settings);
#endif
	new_settings = initial_settings;
	new_settings.c_lflag &= ~ICANON;
	new_settings.c_lflag &= ~ECHO;
	stty(fileno(cin), &new_settings);

#ifdef BB_FEATURE_AUTOWIDTH	
	ioctl(fileno(stdout), TIOCGWINSZ, &win);
	if (win.ws_row > 4) 
	    terminal_height = win.ws_row - 2;
	if (win.ws_col > 0) 
	    terminal_width = win.ws_col - 1;
#endif

	(void) signal(SIGINT, gotsig);
	(void) signal(SIGQUIT, gotsig);
	(void) signal(SIGTERM, gotsig);

#endif
	while ((c = getc(file)) != EOF) {
	    if ( next_page ) {
		int len=0;
		next_page = 0;
		lines=0;
		len = fprintf(stdout, "--More-- ");
		if (file != stdin) {
		    len += fprintf(stdout, "(%d%% of %ld bytes)", 
			(int) (100*( (double) ftell(file) / (double) st.st_size )),
			st.st_size);
		}
		len += fprintf(stdout, "%s",
#ifdef BB_FEATURE_USE_TERMIOS
			""
#else
			"\n"
#endif
			);

		fflush(stdout);
		input = getc( cin);

#ifdef BB_FEATURE_USE_TERMIOS
		/* Erase the "More" message */
		while(--len >= 0)
		    putc('\b', stdout);
		while(++len <= terminal_width)
		    putc(' ', stdout);
		while(--len >= 0)
		    putc('\b', stdout);
		fflush(stdout);
#endif

	    }
	    if (c == '\n' ) {
		switch(input) {
		    case 'q':
			goto end;
		    case '\n':
			/* increment by just one line if we are at 
			 * the end of this line*/
			next_page = 1;
			break;
		}
		if ( ++lines == terminal_height )
		    next_page = 1;
	    }
	    putc(c, stdout);
	}
	fclose(file);
	fflush(stdout);

	argv++;
    } while (--argc > 0);
end:
#ifdef BB_FEATURE_USE_TERMIOS
    gotsig(0);
#endif	
    exit(TRUE);
}

