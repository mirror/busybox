#include "internal.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define BB_MORE_TERM

#ifdef BB_MORE_TERM
	#include <termios.h>
	#include <signal.h>

	FILE *cin;
	struct termios initial_settings, new_settings;

	void gotsig(int sig) { 
		tcsetattr(fileno(cin), TCSANOW, &initial_settings);
		exit(0);
	}
#endif

const char	more_usage[] = "more [file]\n"
"\n"
"\tDisplays a file, one page at a time.\n"
"\tIf there are no arguments, the standard input is displayed.\n";

extern int
more_fn(const struct FileInfo * i)
{
	FILE *	f = stdin;
	int	c;
	int	lines = 0, tlines = 0;
	int	next_page = 0;
	int	rows = 24, cols = 79;
#ifdef BB_MORE_TERM
	long sizeb = 0;
	struct stat st;	
	struct winsize win;
#endif
	
	if ( i ) {
		if (! (f = fopen(i->source, "r") )) {
			name_and_error(i->source);
			return 1;
		}
		fstat(fileno(f), &st);
		sizeb = st.st_size / 100;
	}
		
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

	while ( (c = getc(f)) != EOF ) {
		if ( next_page ) {
			char	garbage;
			int	len;
			tlines += lines;
			lines = 0;
			next_page = 0;		//Percentage is based on bytes, not lines.
			if ( i && i->source )	//It is not very acurate, but still useful.
				len = printf("%s - %%%2ld - line: %d", i->source, (ftell(f) - sizeb - sizeb) / sizeb, tlines);
			else
				len = printf("line: %d", tlines);
				
			fflush(stdout);
#ifndef BB_MORE_TERM		
			read(2, &garbage, 1);
#else				
			do {
				fread(&garbage, 1, 1, cin);	
			} while ((garbage != ' ') && (garbage != '\n'));
			
			if (garbage == '\n') {
				lines = rows;
				tlines -= rows;
			}					
			garbage = 0;				
			//clear line, since tabs don't overwrite.
			while(len-- > 0)	putchar('\b');
			while(len++ < cols)	putchar(' ');
			while(len-- > 0)	putchar('\b');
			fflush(stdout);
#endif								
		}
		putchar(c);
		if ( c == '\n' && ++lines == (rows + 1) )
			next_page = 1;
	}
	if ( f != stdin )
		fclose(f);
#ifdef BB_MORE_TERM
	gotsig(0);
#endif	
	return 0;
}
