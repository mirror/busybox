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

#include "busybox.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>


int xargs_main(int argc, char **argv)
{
	char *in_from_stdin = NULL;
	char *args = NULL;
	char *cmd_to_be_executed = NULL;
	char traceflag = 0;
	int len_args=10, len_cmd_to_be_executed, opt;
	pid_t pid;
	int wpid, status;

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

	/* Store the command to be executed (taken from the command line) */
	if (argc == 0) {
		len_cmd_to_be_executed=6;
		cmd_to_be_executed = xmalloc(len_cmd_to_be_executed);
		strcat(cmd_to_be_executed, "echo");
	} else {
		opt=strlen(*argv);
		len_cmd_to_be_executed = (opt > 10)? opt : 10;
		cmd_to_be_executed = xcalloc(len_cmd_to_be_executed, sizeof(char));
		strcat(cmd_to_be_executed, *argv);
	}


	/* Now, read in one line at a time from stdin, and stroe this to be used later 
	 * as an argument to the command we just stored */
	in_from_stdin = get_line_from_file(stdin);
	for (;in_from_stdin!=NULL;) {
		char *tmp;
		opt = strlen(in_from_stdin);
		len_args += opt + 3;
		args=xrealloc(args, len_args);

		/* Strip out the final \n */
		in_from_stdin[opt-1]=' ';

		/* Replace any tabs with spaces */
		while( (tmp = strchr(in_from_stdin, '\t')) != NULL )
			*tmp=' ';

		/* Strip out any extra intra-word spaces */
		while( (tmp = strstr(in_from_stdin, "  ")) != NULL ) {
			opt = strlen(in_from_stdin);
			memmove(tmp, tmp+1, opt-(tmp-in_from_stdin));
		}

		/* trim trailing whitespace */
		opt = strlen(in_from_stdin) - 1;
		while (isspace(in_from_stdin[opt]))
			opt--;
		in_from_stdin[++opt] = 0;

		/* Strip out any leading whitespace */
		tmp=in_from_stdin;
		while(isspace(*tmp))
			tmp++;

		strcat(args, tmp);
		strcat(args, " ");

		free(in_from_stdin);
		in_from_stdin = get_line_from_file(stdin);
	}

	if (traceflag==1) {
		fputs(cmd_to_be_executed, stderr);
		fputs(args, stderr);
	}

	if ((pid = fork()) == 0) {
		char *cmd[255];
		int i=1;

		//printf("argv[0]='%s'\n", cmd_to_be_executed);
		cmd[0] = cmd_to_be_executed;
		while (--argc && ++argv && *argv ) {
			//printf("argv[%d]='%s'\n", i, *argv);
			cmd[i++]=*argv;
		}
		//printf("argv[%d]='%s'\n", i, args);
		cmd[i++] = args;
		cmd[i] = NULL;
		execvp(cmd_to_be_executed, cmd);

		/* What?  Still here?  Exit with an error */
		fatalError("%s: %s\n", cmd_to_be_executed, strerror(errno));
	}
	/* Wait for a child process to exit */
	wpid = wait(&status);


#ifdef BB_FEATURE_CLEAN_UP
	free(args);
	free(cmd_to_be_executed);
#endif

	if (wpid > 0)
		return (WEXITSTATUS(status));
	else 
		return EXIT_FAILURE;
}
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
