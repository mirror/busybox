/*
 * Mini more implementation for busybox
 *
 * Copyright (C) 1998 by Erik Andersen <andersee@debian.org>
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


/* Turning this off makes things a bit smaller (and less pretty) */
#define BB_MORE_TERM



#include "internal.h"
#include <stdio.h>
#include <signal.h>


const char more_usage[] = "[file ...]";


#ifdef BB_MORE_TERM
    #include <termios.h>
    #include <signal.h>
    #include <sys/ioctl.h>

    FILE *cin;
    struct termios initial_settings, new_settings;

    void gotsig(int sig) { 
	    tcsetattr(fileno(cin), TCSANOW, &initial_settings);
	    exit( TRUE);
    }
#endif

extern int more_main(int argc, char **argv)
{
    int c, lines=0, input;
    int next_page=0, rows = 24;
#ifdef BB_MORE_TERM
    int cols;
    struct winsize win;
#endif
    struct stat st;	
    FILE *file = stdin;

    if ( strcmp(*argv,"--help")==0 || strcmp(*argv,"-h")==0 ) {
	fprintf(stderr, "Usage: %s %s", *argv, more_usage);
	exit(FALSE);
    }
    argc--;
    argv++;

    while (argc-- > 0) {
	    file = fopen(*argv, "r");
	if (file == NULL) {
	    perror("Can't open file");
	    exit(FALSE);
	}
	fstat(fileno(file), &st);
	fprintf(stderr, "hi\n");

#ifdef BB_MORE_TERM
	cin = fopen("/dev/tty", "r");
	tcgetattr(fileno(cin),&initial_settings);
	new_settings = initial_settings;
	new_settings.c_lflag &= ~ICANON;
	new_settings.c_lflag &= ~ECHO;
	tcsetattr(fileno(cin), TCSANOW, &new_settings);
	
	(void) signal(SIGINT, gotsig);

	ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);
	if (win.ws_row > 4)	rows = win.ws_row - 2;
	if (win.ws_col > 0)	cols = win.ws_col - 1;


#endif
	while ((c = getc(file)) != EOF) {
	    if ( next_page ) {
		int len=0;
		next_page = 0;
		lines=0;
		len = fprintf(stdout, "--More-- (%d%% of %ld bytes)%s", 
			(int) (100*( (double) ftell(file) / (double) st.st_size )),
			st.st_size,
#ifdef BB_MORE_TERM
			""
#else
			"\n"
#endif
			);

		fflush(stdout);
		input = getc( stdin);

#ifdef BB_MORE_TERM
		/* Erase the "More" message */
		while(len-- > 0)
		    putc('\b', stdout);
		while(len++ < cols)
		    putc(' ', stdout);
		while(len-- > 0)
		    putc('\b', stdout);
		fflush(stdout);
#endif

	    }
	    if (input=='q')
		goto end;
	    if (input==' ' &&  c == '\n' )
		next_page = 1;
	    if ( c == '\n' && ++lines == (rows + 1) )
		next_page = 1;
	    putc(c, stdout);
	}
	fclose(file);
	fflush(stdout);

	argc--;
	argv++;
    }
end:
#ifdef BB_MORE_TERM
    gotsig(0);
#endif	
    exit(TRUE);
}

