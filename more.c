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

#include "internal.h"
#include <stdio.h>
#include <signal.h>

const char more_usage[] = "[file ...]";

//#define ERASE_STUFF

extern int more_main(int argc, char **argv)
{
    int c, lines=0;
    int next_page=0, rows = 24;
#ifdef ERASE_STUFF
    int cols=79;
#endif
    struct stat st;	
    FILE *file = stdin;

    if ( strcmp(*argv,"--help")==0 || strcmp(*argv,"-h")==0 ) {
	fprintf(stderr, "Usage: %s %s", *argv, more_usage);
	return(FALSE);
    }
    argc--;
    argv++;

    while (argc-- > 0) {
	    file = fopen(*argv, "r");
	if (file == NULL) {
	    perror("Can't open file");
	    return(FALSE);
	}
	fstat(fileno(file), &st);
	fprintf(stderr, "hi\n");

	while ((c = getc(file)) != EOF) {
	    if ( next_page ) {
		int len=0;
		next_page = 0;
		lines=0;
		len = fprintf(stdout, "--More-- (%d%% of %ld bytes)", 
			(int) (100*( (double) ftell(file) / (double) st.st_size )),
			st.st_size);
		fflush(stdout);
		getc( stdin);
#ifdef ERASE_STUFF
		/* Try to erase the "More" message */
		while(len-- > 0)
		    putc('\b', stdout);
		while(len++ < cols)
		    putc(' ', stdout);
		while(len-- > 0)
		    putc('\b', stdout);
		fflush(stdout);
#endif
	    }
	    if ( c == '\n' && ++lines == (rows + 1) )
		next_page = 1;
	    putc(c, stdout);
	}
	fclose(file);
	fflush(stdout);

	argc--;
	argv++;
    }
    return(TRUE);
}


