/* vi: set sw=4 ts=4: */
/*
 * strings implementation for busybox
 *
 * Copyright (c) 1980, 1987
 *	The Regents of the University of California.  All rights reserved.
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
 *
 * Modified for BusyBox by Erik Andersen <andersen@codepoet.org>
 * Badly hacked by Tito Ragusa <farmatito@tiscali.it>
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <ctype.h>
#include "busybox.h"

#define ISSTR(ch)	(isprint(ch) || ch == '\t')

#define WHOLE_FILE		1
#define PRINT_NAME		2
#define PRINT_OFFSET	4
#define SIZE			8

int strings_main(int argc, char **argv)
{
	int n, c, i = 0, status = EXIT_SUCCESS;
	unsigned long opt;
	unsigned long count;
	FILE *file = stdin;
	char *string;
	const char *fmt = "%s: ";
	char *n_arg = "4";
	
	opt = bb_getopt_ulflags (argc, argv, "afon:", &n_arg);
	/* -a is our default behaviour */
	
	argc -= optind;
	argv += optind;

	n = bb_xgetlarg(n_arg, 10, 1, INT_MAX);
	string = xcalloc(n + 1, 1);
	n--;
	
	if ( argc == 0) {
		fmt = "{%s}: ";
		*argv = (char *)bb_msg_standard_input;
		goto PIPE;
	}
	
	do {
		if ((file = bb_wfopen(*argv, "r"))) {
PIPE:
			count = 0;
			do {
				c = fgetc(file);
				if (ISSTR(c)) {
					if (i <= n) {
						string[i]=c;
					} else {
						putchar(c);
					}
					if (i == n) {
						if (opt & PRINT_NAME) {
							printf(fmt, *argv);
						}
						if (opt & PRINT_OFFSET) {
							printf("%7lo ", count - n );
						}
						printf("%s", string);
					}
					i++;
				} else {
					if (i > n) {
						putchar('\n');
					}
					i = 0;
				}
				count++;
			} while (c != EOF);
			bb_fclose_nonstdin(file);
		} else {
			status=EXIT_FAILURE;
		}
	} while ( --argc > 0 );
#ifdef CONFIG_FEATURE_CLEAN_UP	
	free(string);
#endif
	bb_fflush_stdout_and_exit(status);
}

/*
 * Copyright (c) 1980, 1987
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. <BSD Advertising Clause omitted per the July 22, 1999 licensing change
 *		ftp://ftp.cs.berkeley.edu/pub/4bsd/README.Impt.License.Change>
 *
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
