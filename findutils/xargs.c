/*
 * Mini xargs implementation for busybox
 * Only "-prt" options are supported in this version of xargs.
 *
 * (C) 2002 by Vladimir Oleynik <dzo@simtreas.ru>
 * (C) 2003 by Glenn McGrath <bug1@optushome.com.au>
 *
 * Special thanks Mark Whitley for stimul to rewrote :)
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
 *
 * Reference:
 *	http://www.opengroup.org/onlinepubs/007904975/utilities/xargs.html
 *
 *
 * BUGS:
 *	p option doesnt accept user input, should read input from /dev/tty
 *
 *	E option doesnt allow spaces before argument
 *
 *	xargs should terminate if an invocation of a constructed command line
 *  returns an exit status of 255.
 *
 *  exit value of isnt correct 
 *
 *  doesnt print quoted string properly
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "busybox.h"

/*
   This function have special algorithm.
   Don`t use fork and include to main!
*/
static void xargs_exec(char * const * args)
{
	int p;
	int common[4];  /* shared vfork stack */

	common[0] = 0;
	if ((p = vfork()) >= 0) {
		if (p == 0) {
			/* vfork -- child */
			execvp(args[0], args);
			common[0] = errno; /* set error to shared stack */
			_exit(1);
		} else {
			/* vfork -- parent */
			wait(NULL);
			if(common[0]) {
				errno = common[0];
				bb_perror_msg_and_die("%s", args[0]);
			}
		}
	} else {
		bb_perror_msg_and_die("vfork");
	}
}

#define OPT_VERBOSE	0x2
#define OPT_INTERACTIVE	0x4
#define OPT_TERMINATE	0x8
#define OPT_UPTO_NUMBER	0x10
#define OPT_UPTO_SIZE	0x20
#define OPT_EOF_STRING	0x40

int xargs_main(int argc, char **argv)
{
	char *s_max_args = NULL;
	char *s_line_size = NULL;
	unsigned long flg;

	char *eof_string = "_";
	int line_size = LINE_MAX;
	unsigned int max_args = LINE_MAX / 2;

	char *line_buffer = NULL;
	char *line_buffer_ptr_ptr;
	char *old_arg = NULL;

	char **args;
	char *args_entry_ptr;

	int i;
	int a;

	
	bb_opt_complementaly = "pt";

	flg = bb_getopt_ulflags(argc, argv, "+tpxn:s:E::", &s_max_args, &s_line_size, &eof_string);

	if (s_max_args) {
		max_args = bb_xgetularg10(s_max_args);
	}
	if (s_line_size) {
		line_size = bb_xgetularg10(s_line_size);
	}

	a = argc - optind;
	argv += optind;
	if(a==0) {
		/* default behavior is to echo all the filenames */
		*argv = "echo";
		a++;
	}
	/* allocating pointers for execvp: a*arg, arg from stdin, NULL */
	args = xcalloc(a + 2, sizeof(char *));

	/* Store the command to be executed (taken from the command line) */
	for (i = 0; i < a; i++) {
		line_size -= strlen(*argv) + 1;
		args[i] = *argv++;
	}
	if (line_size < 1) {
		bb_error_msg_and_die("can not fit single argument within argument list size limit");
	}

	args[i] = xmalloc(line_size);
	args_entry_ptr = args[i];

	/* Now, read in one line at a time from stdin, and store this 
	 * line to be used later as an argument to the command */
	do {
		char *line_buffer_ptr = NULL;
		unsigned int arg_count = 0;
		unsigned int arg_size = 0;

		*args_entry_ptr = '\0';

		/* Get the required number of entries from stdin */
		do {
			/* (Re)fill the line buffer */
			if (line_buffer == NULL) {
				line_buffer = bb_get_chomped_line_from_file(stdin);
				if (line_buffer == NULL) {
					/* EOF, exit outer loop */
					break;
				}
				line_buffer_ptr = strtok_r(line_buffer, " \t", &line_buffer_ptr_ptr);
			} else {
				if (old_arg) {
					line_buffer_ptr = old_arg;
					old_arg = NULL;
				} else {
					line_buffer_ptr = strtok_r(NULL, " \t", &line_buffer_ptr_ptr);
				}
			}
			/* If no arguments left go back and get another line */
			if (line_buffer_ptr == NULL) {
				free(line_buffer);
				line_buffer = NULL;
				continue;
			}

			if (eof_string && (strcmp(line_buffer_ptr, eof_string) == 0)) {
				/* logical EOF, exit outer loop */
				line_buffer = NULL;
				break;
			}

			/* Check the next argument will fit */
			arg_size += 1 + strlen(line_buffer_ptr);
			if (arg_size > line_size) {
				if ((arg_count == 0) || ((flg & OPT_TERMINATE) && (arg_count != max_args))){
					bb_error_msg_and_die("argument line too long");
				}
				old_arg = line_buffer_ptr;
				break;
			}

			/* Add the entry to our pre-allocated space */
			strcat(args_entry_ptr, line_buffer_ptr);
			strcat(args_entry_ptr, " ");
			arg_count++;
		} while (arg_count < max_args);

		if (*args_entry_ptr != '\0') {
			if(flg & (OPT_VERBOSE | OPT_INTERACTIVE)) {
				for(i=0; args[i]; i++) {
					if(i)
						fputc(' ', stderr);
					fputs(args[i], stderr);
				}

				fputs(((flg & OPT_INTERACTIVE) ? " ?..." : "\n"), stderr);
			}

			if((flg & OPT_INTERACTIVE) == 0 || bb_ask_confirmation() != 0 ) {
				xargs_exec(args);
			}
		}
	} while (line_buffer);

#ifdef CONFIG_FEATURE_CLEAN_UP
	free(args);
#endif
	return 0;
}
