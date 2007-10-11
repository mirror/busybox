/* vi: set sw=4 ts=4: */
/*
 * env implementation for busybox
 *
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
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
 * Modified by Vladimir Oleynik <dzo@simtreas.ru> (C) 2003
 * - correct "-" option usage
 * - multiple "-u unsetenv" support
 * - GNU long option support
 * - use xfunc_error_retval
 */

#include <getopt.h> /* struct option */
extern char **environ;

#include "libbb.h"

#if ENABLE_FEATURE_ENV_LONG_OPTIONS
static const char env_longopts[] ALIGN1 =
	"ignore-environment\0" No_argument       "i"
	"unset\0"              Required_argument "u"
	;
#endif

int env_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int env_main(int argc, char **argv)
{
	/* cleanenv was static - why? */
	char *cleanenv[1];
	char **ep;
	unsigned opt;
	llist_t *unset_env = NULL;

	opt_complementary = "u::";
#if ENABLE_FEATURE_ENV_LONG_OPTIONS
	applet_long_options = env_longopts;
#endif
	opt = getopt32(argv, "+iu:", &unset_env);
	argv += optind;
	if (*argv && LONE_DASH(argv[0])) {
		opt |= 1;
		++argv;
	}
	if (opt & 1) {
		cleanenv[0] = NULL;
		environ = cleanenv;
	} else {
		while (unset_env) {
			unsetenv(unset_env->data);
			unset_env = unset_env->link;
		}
	}

	while (*argv && (strchr(*argv, '=') != NULL)) {
		if (putenv(*argv) < 0) {
			bb_perror_msg_and_die("putenv");
		}
		++argv;
	}

	if (*argv) {
		BB_EXECVP(*argv, argv);
		/* SUSv3-mandated exit codes. */
		xfunc_error_retval = (errno == ENOENT) ? 127 : 126;
		bb_simple_perror_msg_and_die(*argv);
	}

	for (ep = environ; *ep; ep++) {
		puts(*ep);
	}

	fflush_stdout_and_exit(0);
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


