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
#include <fcntl.h>
#include <signal.h>


static const char more_usage[] = "[file ...]";


/* ED: sparc termios is broken: revert back to old termio handling. */
#ifdef BB_MORE_TERM


#if defined (__sparc__)
#      define USE_OLD_TERMIO
#      include <termio.h>
#      include <sys/ioctl.h>
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
	    exit( TRUE);
    }
#endif

extern int more_main(int argc, char **argv)
{
    int c, lines=0, input=0;
    int next_page=0;
    struct stat st;	
    FILE *file;

    if ( strcmp(*argv,"--help")==0 || strcmp(*argv,"-h")==0 ) {
	usage (more_usage);
    }
    argc--;
    argv++;

    while (argc >= 0) {
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

#ifdef BB_MORE_TERM
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
	
	(void) signal(SIGINT, gotsig);

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
#ifdef BB_MORE_TERM
			""
#else
			"\n"
#endif
			);

		fflush(stdout);
		input = getc( cin);

#ifdef BB_MORE_TERM
		/* Erase the "More" message */
		while(len-- > 0)
		    putc('\b', stdout);
		while(len++ < 79)
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
	    if ( c == '\n' && ++lines == 24 )
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

