/* vi: set sw=4 ts=4: */
/*
 * Copyright 1989 - 1991, Julianne Frances Haugh <jockgrrl@austin.rr.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Julianne F. Haugh nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JULIE HAUGH AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JULIE HAUGH OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <ctype.h>
#include "libbb.h"
#ifdef CONFIG_SELINUX
#include <proc_secure.h>
#endif

/* Run SHELL, or DEFAULT_SHELL if SHELL is empty.
   If COMMAND is nonzero, pass it to the shell with the -c option.
   If ADDITIONAL_ARGS is nonzero, pass it to the shell as more
   arguments.  */

void run_shell ( const char *shell, int loginshell, const char *command, const char **additional_args
#ifdef CONFIG_SELINUX
	, security_id_t sid
#endif
)
{
	const char **args;
	int argno = 1;
	int additional_args_cnt = 0;

	for ( args = additional_args; args && *args; args++ )
		additional_args_cnt++;

		args = (const char **) xmalloc (sizeof (char *) * ( 4  + additional_args_cnt ));

	args [0] = bb_get_last_path_component ( bb_xstrdup ( shell ));

	if ( loginshell ) {
		char *args0;
		bb_xasprintf ( &args0, "-%s", args [0] );
		args [0] = args0;
	}

	if ( command ) {
		args [argno++] = "-c";
		args [argno++] = command;
	}
	if ( additional_args ) {
		for ( ; *additional_args; ++additional_args )
			args [argno++] = *additional_args;
	}
	args [argno] = 0;
#ifdef CONFIG_SELINUX
	if(sid)
		execve_secure(shell, (char **) args, environ, sid);
	else
#endif
	execv ( shell, (char **) args );
	bb_perror_msg_and_die ( "cannot run %s", shell );
}
