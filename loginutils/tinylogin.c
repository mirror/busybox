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

#include "busybox.h"

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <syslog.h>
#include <ctype.h>

#include "pwd.h"
#include "grp.h"

#ifdef CONFIG_FEATURE_SHADOWPASSWDS
#include "shadow.h"
#endif

#define DEFAULT_LOGIN_PATH      "/bin:/usr/bin"
#define DEFAULT_ROOT_LOGIN_PATH "/usr/sbin:/bin:/usr/bin:/sbin"


static void xsetenv ( const char *key, const char *value )
{
	if ( setenv ( key, value, 1 ))
		error_msg_and_die ( "out of memory" );
}

/* Become the user and group(s) specified by PW.  */

void change_identity ( const struct passwd *pw )
{
	if ( initgroups ( pw-> pw_name, pw-> pw_gid ) == -1 )
		perror_msg_and_die ( "cannot set groups" );
	endgrent ( );

	if ( setgid ( pw-> pw_gid ))
		perror_msg_and_die ( "cannot set group id" );
	if ( setuid ( pw->pw_uid ))
		perror_msg_and_die ( "cannot set user id" );
}

/* Run SHELL, or DEFAULT_SHELL if SHELL is empty.
   If COMMAND is nonzero, pass it to the shell with the -c option.
   If ADDITIONAL_ARGS is nonzero, pass it to the shell as more
   arguments.  */

void run_shell ( const char *shell, int loginshell, const char *command, const char **additional_args )
{
	const char **args;
	int argno = 1;
	int additional_args_cnt = 0;
	
	for ( args = additional_args; args && *args; args++ )
		additional_args_cnt++;

	if ( additional_args )
		args = (const char **) xmalloc (sizeof (char *) * ( 4  + additional_args_cnt ));
	else
		args = (const char **) xmalloc (sizeof (char *) * 4 );
		
	args [0] = get_last_path_component ( xstrdup ( shell ));
	
	if ( loginshell ) {
		char *args0 = xmalloc ( xstrlen ( args [0] ) + 2 );
		args0 [0] = '-';
		strcpy ( args0 + 1, args [0] );
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
	execv ( shell, (char **) args );
	perror_msg_and_die ( "cannot run %s", shell );
}

/* Return 1 if SHELL is a restricted shell (one not returned by
   getusershell), else 0, meaning it is a standard shell.  */

int restricted_shell ( const char *shell )
{
	char *line;

	setusershell ( );
	while (( line = getusershell ( ))) {
		if (( *line != '#' ) && ( strcmp ( line, shell ) == 0 ))
			break;
	}
	endusershell ( );
	return line ? 0 : 1;
}

/* Update `environ' for the new shell based on PW, with SHELL being
   the value for the SHELL environment variable.  */

void setup_environment ( const char *shell, int loginshell, int changeenv, const struct passwd *pw )
{
	if ( loginshell ) {
		char *term;
	
		/* Change the current working directory to be the home directory
		 * of the user.  It is a fatal error for this process to be unable
		 * to change to that directory.  There is no "default" home
		 * directory.
		 * Some systems default to HOME=/
		 */		 
		if ( chdir ( pw-> pw_dir )) {
			if ( chdir ( "/" )) {
				syslog ( LOG_WARNING, "unable to cd to %s' for user %s'\n", pw-> pw_dir, pw-> pw_name );
				error_msg_and_die ( "cannot cd to home directory or /" );
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

/* Ask the user for a password.
   Return 1 if the user gives the correct password for entry PW,
   0 if not.  Return 1 without asking for a password if run by UID 0
   or if PW has an empty password.  */

int correct_password ( const struct passwd *pw )
{
	char *unencrypted, *encrypted, *correct;
	
#ifdef CONFIG_FEATURE_SHADOWPASSWDS
	if (( strcmp ( pw-> pw_passwd, "x" ) == 0 ) || ( strcmp ( pw-> pw_passwd, "*" ) == 0 )) {
		struct spwd *sp = getspnam ( pw-> pw_name );
		
		if ( !sp )
			error_msg_and_die ( "no valid shadow password" );
		
		correct = sp-> sp_pwdp;
	}
	else
#endif
    	correct = pw-> pw_passwd;

	if ( correct == 0 || correct[0] == '\0' )
		return 1;

	unencrypted = getpass ( "Password: " );
	if ( !unencrypted )
	{
		fputs ( "getpass: cannot open /dev/tty\n", stderr );
		return 0;
	}
	encrypted = crypt ( unencrypted, correct );
	memset ( unencrypted, 0, xstrlen ( unencrypted ));
	return ( strcmp ( encrypted, correct ) == 0 ) ? 1 : 0;
}
