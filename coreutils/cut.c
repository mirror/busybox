/* vi: set sw=4 ts=4: */
/*
 * cut implementation for busybox
 *
 * Copyright (c) Michael J. Holme
 *
 * This version of cut is adapted from Minix cut and was modified 
 * by Erik Andersen <andersee@debian.org> to be used in busybox.
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
 * Original copyright notice is retained at the end of this file.
 */

#include "internal.h"
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#define BB_DECLARE_EXTERN
#define bb_need_help
#include "messages.c"

#define MAX_FIELD	80			/* Pointers to the beginning of each field
								   * are stored in columns[], if a line holds
								   * more than MAX_FIELD columns the array
								   * boundary is exceed. But unlikely at 80 */

#define MAX_ARGS	32			/* Maximum number of fields following -f or
								   * -c switches                                                      */
int args[MAX_ARGS * 2];
int num_args;

/* Lots of new defines, should easen maintainance...			*/
#define DUMP_STDIN	0			/* define for mode: no options   */
#define OPTIONF		1			/* define for mode: option -f    */
#define OPTIONC		2			/* define for mode: option -c    */
#define OPTIONB		3			/* define for mode: option -b    */
#define NOTSET		0			/* option not selected       */
#define SET		1				/* option selected       */

/* Defines for the warnings						*/
#define DELIMITER_NOT_APPLICABLE	0
#define OVERRIDING_PREVIOUS_MODE	1
#define OPTION_NOT_APPLICABLE		2
#define UNKNOWN_OPTION			3
#define FILE_NOT_READABLE		4

/* Defines for the fatal errors						*/
#define SYNTAX_ERROR				101
#define POSITION_ERROR				102
#define LINE_TO_LONG_ERROR			103
#define RANGE_ERROR					104
#define MAX_FIELDS_EXEEDED_ERROR	105
#define MAX_ARGS_EXEEDED_ERROR		106


int mode;						/* 0 = dump stdin to stdout, 1=-f, 2=-c   */
char delim = '\t';				/* default delimiting character   */
FILE *fd;
char *name;
char line[BUFSIZ];
int exit_status;

int cut_main(int argc, char **argv);
void warn(int warn_number, char *option);
void cuterror(int err);
void get_args(void);
void cut(void);

void warn(int warn_number, char *option)
{
	static char *warn_msg[] = {
		"%s: Option -d allowed only with -f\n",
		"%s: -%s overrides earlier option\n",
		"%s: -%s not allowed in current mode\n",
		"%s: Cannot open %s\n"
	};

	fprintf(stderr, warn_msg[warn_number], name, option);
	exit_status = warn_number + 1;

}

void cuterror(int err)
{
	static char *err_mes[] = {
		"%s: syntax error\n",
		"%s: position must be >0\n",
		"%s: line longer than BUFSIZ\n",
		"%s: range must not decrease from left to right\n",
		"%s: MAX_FIELD exceeded\n",
		"%s: MAX_ARGS exceeded\n"
	};

	fprintf(stderr, err_mes[err - 101], name);
	exit(err);
}


void get_args()
{
	int i = 0;
	int arg_ptr = 0;
	int flag;

	num_args = 0;
	do {
		if (num_args == MAX_ARGS)
			cuterror(MAX_ARGS_EXEEDED_ERROR);
		if (!isdigit(line[i]) && line[i] != '-')
			cuterror(SYNTAX_ERROR);

		args[arg_ptr] = 1;
		args[arg_ptr + 1] = BUFSIZ;
		flag = 1;

		while (line[i] != ',' && line[i] != 0) {
			if (isdigit(line[i])) {
				args[arg_ptr] = 0;
				while (isdigit(line[i]))
					args[arg_ptr] = 10 * args[arg_ptr] + line[i++] - '0';
				if (!args[arg_ptr])
					cuterror(POSITION_ERROR);
				arg_ptr++;
			}
			if (line[i] == '-') {
				arg_ptr |= 1;
				i++;
				flag = 0;
			}
		}
		if (flag && arg_ptr & 1)
			args[arg_ptr] = args[arg_ptr - 1];
		if (args[num_args * 2] > args[num_args * 2 + 1])
			cuterror(RANGE_ERROR);
		num_args++;
		arg_ptr = num_args * 2;
	}
	while (line[i++]);
}


void cut()
{
	int i, j, length, maxcol=0;
	char *columns[MAX_FIELD];

	while (fgets(line, BUFSIZ, fd)) {
		length = strlen(line) - 1;
		*(line + length) = 0;
		switch (mode) {
		case DUMP_STDIN:
			printf("%s", line);
			break;
		case OPTIONF:
			columns[maxcol++] = line;
			for (i = 0; i < length; i++) {
				if (*(line + i) == delim) {
					*(line + i) = 0;
					if (maxcol == MAX_FIELD)
						cuterror(MAX_FIELDS_EXEEDED_ERROR);
					columns[maxcol] = line + i + 1;
					maxcol++;
				}
			}
			if (maxcol != 1) { 
				for (i = 0; i < num_args; i++) {
					for (j = args[i * 2]; j <= args[i * 2 + 1]; j++)
						if (j <= maxcol) {
							printf("%s", columns[j - 1]);
							if (i != num_args - 1 || j != args[i * 2 + 1])
								putchar(delim);
						}
				}
			}
			break;
		case OPTIONC:
			for (i = 0; i < num_args; i++) {
				for (j = args[i * 2];
					 j <= (args[i * 2 + 1] >
						   length ? length : args[i * 2 + 1]); j++)
					putchar(*(line + j - 1));
			}
		}
		if (maxcol != 1)
			putchar('\n');
	}
}


int cut_main(int argc, char **argv)
{
	int i = 1;
	int numberFilenames = 0;

	name = argv[0];

	if (argc == 1 || strcmp(argv[1], dash_dash_help)==0)
		usage( "cut [OPTION]... [FILE]...\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
				"\nPrints selected fields from each input FILE to standard output.\n\n"
				"Options:\n"
				"\t-b LIST\tOutput only bytes from LIST\n"
				"\t-c LIST\tOutput only characters from LIST\n"
				"\t-d DELIM\tUse DELIM instead of tab as the field delimiter\n"
				"\t-f N\tPrint only these fields\n"
				"\t-n\tIgnored\n"
#endif
				);

	while (i < argc) {
		if (argv[i][0] == '-') {
			switch (argv[i++][1]) {
			case 'd':
				if (mode == OPTIONC || mode == OPTIONB)
					warn(DELIMITER_NOT_APPLICABLE, "d");
				delim = argv[i++][0];
				break;
			case 'f':
				sprintf(line, "%s", argv[i++]);
				if (mode == OPTIONC || mode == OPTIONB)
					warn(OVERRIDING_PREVIOUS_MODE, "f");
				mode = OPTIONF;
				break;
			case 'b':
				sprintf(line, "%s", argv[i++]);
				if (mode == OPTIONF || mode == OPTIONC)
					warn(OVERRIDING_PREVIOUS_MODE, "b");
				mode = OPTIONB;
				break;
			case 'c':
				sprintf(line, "%s", argv[i++]);
				if (mode == OPTIONF || mode == OPTIONB)
					warn(OVERRIDING_PREVIOUS_MODE, "c");
				mode = OPTIONC;
				break;
			case '\0':			/* - means: read from stdin      */
				numberFilenames++;
				break;
			case 'n':			/* needed for Posix, but no effect here  */
				if (mode != OPTIONB)
					warn(OPTION_NOT_APPLICABLE, "n");
				break;
			default:
				warn(UNKNOWN_OPTION, &(argv[i - 1][1]));
			}
		} else {
			i++;
			numberFilenames++;
		}
	}

/* Here follow the checks, if the selected options are reasonable.	*/
	if (mode == OPTIONB)		/* since in Minix char := byte       */
		mode = OPTIONC;
	get_args();
	if (numberFilenames != 0) {
		i = 1;
		while (i < argc) {
			if (argv[i][0] == '-') {
				switch (argv[i][1]) {
				case 'f':
				case 'c':
				case 'b':
				case 'd':
					i += 2;
					break;
				case 'n':
				case 'i':
				case 's':
					i++;
					break;
				case '\0':
					fd = stdin;
					i++;
					cut();
					break;
				default:
					i++;
				}
			} else {
				if ((fd = fopen(argv[i++], "r")) == NULL) {
					warn(FILE_NOT_READABLE, argv[i - 1]);
				} else {
					cut();
					fclose(fd);
				}
			}
		}
	} else {
		fd = stdin;
		cut();
	}

	exit(exit_status);
}

/* cut - extract columns from a file or stdin. 	Author: Michael J. Holme
 *
 *	Copyright 1989, Michael John Holme, All rights reserved.
 *	This code may be freely distributed, provided that this notice
 *	remains intact.
 *
 *	V1.1: 6th September 1989
 *
 *	Bugs, criticisms, etc,
 *      c/o Mark Powell
 *          JANET sq79@uk.ac.liv
 *          ARPA  sq79%liv.ac.uk@nsfnet-relay.ac.uk
 *          UUCP  ...!mcvax!ukc!liv.ac.uk!sq79
 *-------------------------------------------------------------------------
 *	Changed for POSIX1003.2/Draft10 conformance
 *	Thomas Brupbacher (tobr@mw.lpc.ethz.ch), September 1990.
 *	Changes:
 *	    - separation of error messages ( stderr) and output (stdout).
 *	    - support for -b and -n (no effect, -b acts as -c)
 *	    - support for -s
 *-------------------------------------------------------------------------
 */

/*
 * Copyright (c) 1987,1997, Prentice Hall
 * All rights reserved.
 * 
 * Redistribution and use of the MINIX operating system in source and
 * binary forms, with or without modification, are permitted provided
 * that the following conditions are met:
 * 
 * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 * 
 * Neither the name of Prentice Hall nor the names of the software
 * authors or contributors may be used to endorse or promote
 * products derived from this software without specific prior
 * written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS, AUTHORS, AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PRENTICE HALL OR ANY AUTHORS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */


