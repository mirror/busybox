/* vi: set sw=4 ts=4: */

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>
#include <utmp.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <ctype.h>
#include <time.h>

#include "busybox.h"



/* The shell to run if none is given in the user's passwd entry.  */
#define DEFAULT_SHELL "/bin/sh"
#define DEFAULT_USER  "root"

//#define SYSLOG_SUCCESS
#define SYSLOG_FAILURE


#if defined( SYSLOG_SUCCESS ) || defined( SYSLOG_FAILURE )
/* Log the fact that someone has run su to the user given by PW;
   if SUCCESSFUL is nonzero, they gave the correct password, etc.  */

static void log_su ( const struct passwd *pw, int successful )
{
	const char *old_user, *tty;

#if !defined( SYSLOG_SUCESS )
	if ( successful )
		return;
#endif
#if !defined( SYSLOG_FAILURE )
	if ( !successful )
		return;
#endif

	if ( pw-> pw_uid ) // not to root -> ignored
		return;

	/* The utmp entry (via getlogin) is probably the best way to identify
	   the user, especially if someone su's from a su-shell.  */
	old_user = getlogin ( );
	if ( !old_user ) {
		/* getlogin can fail -- usually due to lack of utmp entry. Resort to getpwuid.  */
		struct passwd *pwd = getpwuid ( getuid ( ));
		old_user = ( pwd ? pwd-> pw_name : "" );
	}
	
	tty = ttyname ( 2 );

	openlog ( "su", 0, LOG_AUTH );
	syslog ( LOG_NOTICE, "%s%s on %s", successful ? "" : "FAILED SU ", old_user, tty ? tty : "none" );
}
#endif



int su_main ( int argc, char **argv )
{
	int flag;
	int opt_preserve = 0;
	int opt_loginshell = 0;
	char *opt_shell = 0;
	char *opt_command = 0;
	char *opt_username = DEFAULT_USER;
	char **opt_args = 0;
	struct passwd *pw, pw_copy;


	while (( flag = getopt ( argc, argv, "c:lmps:" )) != -1 ) {
		switch ( flag ) {
		case 'c':
			opt_command = optarg;
			break;
		case 'm':
		case 'p':
			opt_preserve = 1;
			break;
		case 's':
			opt_shell = optarg;
			break;
		case 'l':
			opt_loginshell = 1;
			break;
		default:
			show_usage ( );
			break;
		}
	}

	if (( optind < argc ) && ( argv [optind][0] == '-' ) && ( argv [optind][1] == 0 )) {
		opt_loginshell = 1;
		++optind;
    }

	/* get user if specified */
	if ( optind < argc ) 
		opt_username = argv [optind++];

	if ( optind < argc )
		opt_args = argv + optind;
		
		
	pw = getpwnam ( opt_username );
	if ( !pw )
		error_msg_and_die ( "user %s does not exist", opt_username );
		
	/* Make sure pw->pw_shell is non-NULL.  It may be NULL when NEW_USER
	   is a username that is retrieved via NIS (YP), but that doesn't have
	   a default shell listed.  */
	if ( !pw-> pw_shell || !pw->pw_shell [0] )
		pw-> pw_shell = (char *) DEFAULT_SHELL;

	/* Make a copy of the password information and point pw at the local
	   copy instead.  Otherwise, some systems (e.g. Linux) would clobber
	   the static data through the getlogin call from log_su.  */
	pw_copy = *pw;
	pw = &pw_copy;
	pw-> pw_name  = xstrdup ( pw-> pw_name );
	pw-> pw_dir   = xstrdup ( pw-> pw_dir );
	pw-> pw_shell = xstrdup ( pw-> pw_shell );

	if (( getuid ( ) == 0 ) || correct_password ( pw )) 
		log_su ( pw, 1 );
	else {
		log_su ( pw, 0 );
		error_msg_and_die ( "incorrect password" );
	}

	if ( !opt_shell && opt_preserve )
		opt_shell = getenv ( "SHELL" );

	if ( opt_shell && getuid ( ) && restricted_shell ( pw-> pw_shell ))
	{
		/* The user being su'd to has a nonstandard shell, and so is
		   probably a uucp account or has restricted access.  Don't
		   compromise the account by allowing access with a standard
		   shell.  */
		fputs ( "using restricted shell\n", stderr );
		opt_shell = 0;
	}

	if ( !opt_shell )
		opt_shell = xstrdup ( pw-> pw_shell );

	change_identity ( pw );	
	setup_environment ( opt_shell, opt_loginshell, !opt_preserve, pw );
	run_shell ( opt_shell, opt_loginshell, opt_command, (const char**)opt_args );
	
	return EXIT_FAILURE;
}
