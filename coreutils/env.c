/* vi: set sw=4 ts=4: */
/*
 * env implementation for busybox
 *
 * Copyright (c) 1988, 1993, 1994
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
 */

/* BB_AUDIT SUSv3 compliant */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/env.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Fixed bug involving exit return codes if execvp fails.  Also added
 * output error checking.
 */

/*
 * Modified by Vladimir Oleynik <andersen@codepoet.org> (C) 2003
 * - corretion "-" option usage
 * - multiple "-u unsetenv" support
 * - GNU long option support
 * - save errno after exec failed before bb_perror_msg()
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include "busybox.h"


static const struct option env_long_options[] = {
	{ "ignore-environment", 0, NULL, 'i' },
	{ "unset", 1, NULL, 'u' },
	{ 0, 0, 0, 0 }
};

extern int env_main(int argc, char** argv)
{
	char **ep, *p;
	char *cleanenv[1] = { NULL };
	unsigned long opt;
	llist_t *unset_env = NULL;
	extern char **environ;

	bb_opt_complementaly = "u*";
	bb_applet_long_options = env_long_options;

	opt = bb_getopt_ulflags(argc, argv, "+iu:", &unset_env);

	argv += optind;
	if (*argv && (argv[0][0] == '-') && !argv[0][1]) {
		opt |= 1;
		++argv;
	}

	if(opt & 1)
		environ = cleanenv;
	else if(opt & 2) {
		while(unset_env) {
			unsetenv(unset_env->data);
			unset_env = unset_env->link;
		}
	}

	while (*argv && ((p = strchr(*argv, '=')) != NULL)) {
		if (putenv(*argv) < 0) {
			bb_perror_msg_and_die("putenv");
		}
		++argv;
	}

	if (*argv) {
		int er;

		execvp(*argv, argv);
		er = errno;
		bb_perror_msg("%s", *argv);     /* Avoid multibyte problems. */
		return (er == ENOENT) ? 127 : 126;   /* SUSv3-mandated exit codes. */
	}

	for (ep = environ; *ep; ep++) {
		puts(*ep);
	}

	bb_fflush_stdout_and_exit(0);
}

/*
 * Copyright (c) 1988, 1993, 1994
 *      The Regents of the University of California.  All rights reserved.
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


