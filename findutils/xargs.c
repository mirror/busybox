/* vi: set sw=4 ts=4: */
/*
 * Mini xargs implementation for busybox
 *
 * Copyright (C) 2000 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <ctype.h>

/* get_sh_safe_line_from_file() - This function reads an entire line from a text file
 * up to a newline. It returns a malloc'ed char * which must be stored and
 * free'ed  by the caller. */
extern char *get_sh_safe_line_from_file(FILE *file)
{
	static const int GROWBY = 80; /* how large we will grow strings by */

	char ch, last_ch = 0;
	char tmp[]=" ";
	int idx = 0;
	char *linebuf = NULL;
	int linebufsz = 0;

	while (1) {
		ch = fgetc(file);
		if (ch == EOF)
			break;
		
		/* grow the line buffer as necessary */
		while (idx > linebufsz-4)
			linebuf = xrealloc(linebuf, linebufsz += GROWBY);

		/* Remove any extra spaces */
		if (last_ch == ' ' && ch == ' ')
			continue;

		/* Replace any tabs with spaces */
		if (ch == '\t')
			ch=' ';

		/* Escape any characters that are treated specially by /bin/sh */
		*tmp=ch;
		if (strpbrk(tmp, "\\~`!$^&*()=|{}[];\"'<>?#") != NULL && last_ch!='\\') {
			linebuf[idx++] = '\\';
		}

		linebuf[idx++] = ch;
		last_ch=ch;

		if (ch == '\n')
			break;
	}
	if (idx == 0 && last_ch!=0)
		linebuf[idx++]=' ';

	if (idx == 0)
		return NULL;

	linebuf[idx] = 0;
	return linebuf;
}



int xargs_main(int argc, char **argv)
{
	char *in_from_stdin = NULL;
	char *args_from_cmdline = NULL;
	char *cmd_to_be_executed = NULL;
	char traceflag = 0;
	int len_args_from_cmdline, len_cmd_to_be_executed, len, opt;

	/* Note that we do not use getopt here, since
	 * we only want to interpret initial options,
	 * not options passed to commands */
	while (--argc && **(++argv) == '-') {
		while (*++(*argv)) {
			switch (**argv) {
				case 't':
					traceflag=1;
					break;
				default:
					fatalError(xargs_usage);
				}
		}
	}

	/* Store the command and arguments to be executed (from the command line) */
	if (argc == 0) {
		len_args_from_cmdline = 6;
		args_from_cmdline = xmalloc(len_args_from_cmdline);
		strcat(args_from_cmdline, "echo ");
	} else {
		opt=strlen(*argv);
		len_args_from_cmdline = (opt > 10)? opt : 10;
		args_from_cmdline = xcalloc(len_args_from_cmdline, sizeof(char));
		while (argc-- > 0) {
			if (strlen(*argv) + strlen(args_from_cmdline) >
				len_args_from_cmdline) {
				len_args_from_cmdline += strlen(*argv);
				args_from_cmdline =
					xrealloc(args_from_cmdline,
							 len_args_from_cmdline+1);
			}
			strcat(args_from_cmdline, *argv);
			strcat(args_from_cmdline, " ");
			++argv;
		}
	}

	/* Set up some space for the command to be executed to be held in */
	len_cmd_to_be_executed=10;
	cmd_to_be_executed = xcalloc(len_cmd_to_be_executed, sizeof(char));
	strcpy(cmd_to_be_executed, args_from_cmdline);

	/* Now, read in one line at a time from stdin, and run command+args on it */
	in_from_stdin = get_sh_safe_line_from_file(stdin);
	for (;in_from_stdin!=NULL;) {
		char *tmp;
		opt = strlen(in_from_stdin);
		len = opt + len_args_from_cmdline;
		len_cmd_to_be_executed+=len+3;
		cmd_to_be_executed=xrealloc(cmd_to_be_executed, len_cmd_to_be_executed);

		/* Strip out the final \n */
		in_from_stdin[opt-1]=' ';
			
		/* trim trailing whitespace */
		opt = strlen(in_from_stdin) - 1;
		while (isspace(in_from_stdin[opt]))
			opt--;
		in_from_stdin[++opt] = 0;

		/* Strip out any leading whitespace */
		tmp=in_from_stdin;
		while(isspace(*tmp))
			tmp++;

		strcat(cmd_to_be_executed, tmp);
		strcat(cmd_to_be_executed, " ");
	
		free(in_from_stdin);
		in_from_stdin = get_sh_safe_line_from_file(stdin);
	}

	if (traceflag==1)
		fputs(cmd_to_be_executed, stderr);

	if ((system(cmd_to_be_executed) != 0) && (errno != 0))
		fatalError("%s", strerror(errno));


#ifdef BB_FEATURE_CLEAN_UP
	free(args_from_cmdline);
	free(cmd_to_be_executed);
#endif

	return 0;
}
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/

