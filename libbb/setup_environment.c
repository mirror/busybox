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



#define DEFAULT_LOGIN_PATH      "/bin:/usr/bin"
#define DEFAULT_ROOT_LOGIN_PATH "/usr/sbin:/bin:/usr/bin:/sbin"

static void xsetenv ( const char *key, const char *value )
{
	    if ( setenv ( key, value, 1 ))
				bb_error_msg_and_die (bb_msg_memory_exhausted);
}

void setup_environment ( const char *shell, int loginshell, int changeenv, const struct passwd *pw )
{
	if ( loginshell ) {
		const char *term;
	
		/* Change the current working directory to be the home directory
		 * of the user.  It is a fatal error for this process to be unable
		 * to change to that directory.  There is no "default" home
		 * directory.
		 * Some systems default to HOME=/
		 */		 
		if ( chdir ( pw-> pw_dir )) {
			if ( chdir ( "/" )) {
				syslog ( LOG_WARNING, "unable to cd to %s' for user %s'\n", pw-> pw_dir, pw-> pw_name );
				bb_error_msg_and_die ( "cannot cd to home directory or /" );
			}
			fputs ( "warning: cannot change to home directory\n", stderr );
		}

		/* Leave TERM unchanged.  Set HOME, SHELL, USER, LOGNAME, PATH.
		   Unset all other environment variables.  */
		term = getenv ("TERM");
		clearenv ( );
		if ( term )
			xsetenv ( "TERM", term );
		xsetenv ( "HOME",    pw-> pw_dir );
		xsetenv ( "SHELL",   shell );
		xsetenv ( "USER",    pw-> pw_name );
		xsetenv ( "LOGNAME", pw-> pw_name );
		xsetenv ( "PATH",    ( pw-> pw_uid ? DEFAULT_LOGIN_PATH : DEFAULT_ROOT_LOGIN_PATH ));
	}
	else if ( changeenv ) {
		/* Set HOME, SHELL, and if not becoming a super-user,
	   	   USER and LOGNAME.  */
		xsetenv ( "HOME",  pw-> pw_dir );
		xsetenv ( "SHELL", shell );
		if  ( pw-> pw_uid ) {
			xsetenv ( "USER",    pw-> pw_name );
			xsetenv ( "LOGNAME", pw-> pw_name );
		}
	}
}

