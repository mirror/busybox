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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "busybox.h"

extern int 
echo_main(int argc, char** argv)
{
	int nflag = 0;
	int eflag = 0;

	/* Skip argv[0]. */
	argc--;
	argv++;

	while (argc > 0 && *argv[0] == '-')
	{
		register char *temp;
		register int ix;

		/*
		 * If it appears that we are handling options, then make sure
		 * that all of the options specified are actually valid.
		 * Otherwise, the string should just be echoed.
		 */
		temp = argv[0] + 1;

		for (ix = 0; temp[ix]; ix++)
		{
			if (strrchr("neE", temp[ix]) == 0)
				goto just_echo;
		}

		if (!*temp)
			goto just_echo;

		/*
		 * All of the options in temp are valid options to echo.
		 * Handle them.
		 */
		while (*temp)
		{
			if (*temp == 'n')
				nflag = 1;
			else if (*temp == 'e')
				eflag = 1;
			else if (*temp == 'E')
				eflag = 0;
			else
				goto just_echo;

			temp++;
		}
		argc--;
		argv++;
	}

just_echo:
	while (argc > 0) {
		const char *arg = argv[0];
		register int c;

		while ((c = *arg++)) {

			/* Check for escape sequence. */
			if (c == '\\' && eflag && *arg) {
				if (*arg == 'c') {
					/* '\c' means cancel newline. */
					nflag = 1;
					arg++;
					continue;
				} else {
					c = process_escape_sequence(&arg);
				}
			}

			putchar(c);
		}
		argc--;
		argv++;
		if (argc > 0)
			putchar(' ');
	}
	if (!nflag)
		putchar('\n');
	fflush(stdout);

	return EXIT_SUCCESS;
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
