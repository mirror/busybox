/* vi: set sw=4 ts=4: */
/*
 * echo implementation for busybox
 *
 * Copyright (c) 1991, 1993
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
 */

/* BB_AUDIT SUSv3 compliant -- unless configured as fancy echo. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/echo.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Because of behavioral differences, implemented configureable SUSv3
 * or 'fancy' gnu-ish behaviors.  Also, reduced size and fixed bugs.
 * 1) In handling '\c' escape, the previous version only suppressed the
 *     trailing newline.  SUSv3 specifies _no_ output after '\c'.
 * 2) SUSv3 specifies that octal escapes are of the form \0{#{#{#}}}.
 *    The previous version version did not allow 4-digit octals.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "busybox.h"

extern int echo_main(int argc, char** argv)
{
#ifndef CONFIG_FEATURE_FANCY_ECHO
#define eflag '\\'
	++argv;
#else
	const char *p;
	int nflag = 1;
	int eflag = 0;

	while (*++argv && (**argv == '-')) {
		/* If it appears that we are handling options, then make sure
		 * that all of the options specified are actually valid.
		 * Otherwise, the string should just be echoed.
		 */

		if (!*(p = *argv + 1)) {	/* A single '-', so echo it. */
			goto just_echo;
		}

		do {
			if (strrchr("neE", *p) == 0) {
				goto just_echo;
			}
		} while (*++p);

		/* All of the options in this arg are valid, so handle them. */
		p = *argv + 1;
		do {
			if (*p == 'n') {
				nflag = 0;
			} else if (*p == 'e') {
				eflag = '\\';
			} else {
				eflag = 0;
			}
		} while (*++p);
	}

just_echo:
#endif
	while (*argv) {
		register int c;

		while ((c = *(*argv)++)) {
			if (c == eflag) {	/* Check for escape seq. */
				if (**argv == 'c') {
					/* '\c' means cancel newline and
					 * ignore all subsequent chars. */
					goto DONE;
				}
#ifndef CONFIG_FEATURE_FANCY_ECHO
				/* SUSv3 specifies that octal escapes must begin with '0'. */
				if (((unsigned int)(**argv - '1')) >= 7)
#endif
				{
					/* Since SUSv3 mandates a first digit of 0, 4-digit octals
					* of the form \0### are accepted. */
					if ((**argv == '0') && (((unsigned int)(argv[0][1] - '0')) < 8)) {
						(*argv)++;
					}
					/* bb_process_escape_sequence can handle nul correctly */
					c = bb_process_escape_sequence((const char **) argv);
				}
			}
			putchar(c);
		}

		if (*++argv) {
			putchar(' ');
		}
	}

#ifdef CONFIG_FEATURE_FANCY_ECHO
	if (nflag) {
		putchar('\n');
	}
#else
	putchar('\n');
#endif

DONE:
	bb_fflush_stdout_and_exit(EXIT_SUCCESS);
}

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
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
 *	California, Berkeley and its contributors.
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
 *
 *	@(#)echo.c	8.1 (Berkeley) 5/31/93
 */
