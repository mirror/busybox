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
#define DEFAULT_USER  "root"

//#define SYSLOG_SUCCESS
#define SYSLOG_FAILURE


#if defined( SYSLOG_SUCCESS ) || defined( SYSLOG_FAILURE )
/* Log the fact that someone has run su */

# if defined( SYSLOG_SUCCESS ) && defined( SYSLOG_FAILURE )
static void log_su (const char *successful, const char *old_user, const char *tty)
{
	syslog ( LOG_NOTICE, "%s%s on %s", successful, old_user, tty);
}
#  define log_su_successful(cu, u, tty) if(!cu) log_su("", u, tty)
#  define log_su_failure(cu, u, tty)    if(!cu) log_su("FAILED SU ", u, tty)
# else
	/* partial logging */
#  if !defined( SYSLOG_SUCESS )
#   define log_su_successful(cu, u, tty)
#   define log_su_failure(cu, u, t) if(!cu) \
			syslog(LOG_NOTICE, "FAILED SU %s on %s", u, t)
#  else
#   define log_su_successful(cu, u, t) if(!cu) \
			syslog(LOG_NOTICE, "%s on %s", u, t)
#   define log_su_failure(cu, u, tty)
#  endif
# endif
#else
	/* logging not used */
# define log_su_successful(cu, u, tty)
# define log_su_failure(cu, u, tty)
#endif


int su_main ( int argc, char **argv )
{
	unsigned long flags;
	int opt_preserve;
	int opt_loginshell;
	char *opt_shell = 0;
	char *opt_command = 0;
	char *opt_username = DEFAULT_USER;
	char **opt_args = 0;
	struct passwd *pw;
	uid_t cur_uid = getuid();

#if defined( SYSLOG_SUCCESS ) || defined( SYSLOG_FAILURE )
	const char *tty;
	const char *old_user;
#endif

	flags = bb_getopt_ulflags(argc, argv, "mplc:s:",
						  &opt_command, &opt_shell);
	opt_preserve = flags & 3;
	opt_loginshell = (flags & 4 ? 1 : 0);

	if (optind < argc  && argv[optind][0] == '-' && argv[optind][1] == 0) {
		opt_loginshell = 1;
		++optind;
    }

	/* get user if specified */
	if ( optind < argc )
		opt_username = argv [optind++];

	if ( optind < argc )
		opt_args = argv + optind;

#if defined( SYSLOG_SUCCESS ) || defined( SYSLOG_FAILURE )
#ifdef CONFIG_FEATURE_UTMP
	/* The utmp entry (via getlogin) is probably the best way to identify
	   the user, especially if someone su's from a su-shell.  */
	old_user = getlogin ( );
	if ( !old_user )
#endif
		{
		/* getlogin can fail -- usually due to lack of utmp entry. Resort to getpwuid.  */
		pw = getpwuid ( cur_uid );
		old_user = ( pw ? pw->pw_name : "" );
	}
	tty = ttyname ( 2 );
	if(!tty)
		tty = "none";

	openlog ( bb_applet_name, 0, LOG_AUTH );
#endif

	pw = getpwnam ( opt_username );
	if ( !pw )
		bb_error_msg_and_die ( "user %s does not exist", opt_username );

	/* Make sure pw->pw_shell is non-NULL.  It may be NULL when NEW_USER
	   is a username that is retrieved via NIS (YP), but that doesn't have
	   a default shell listed.  */
	if ( !pw-> pw_shell || !pw->pw_shell [0] )
		pw-> pw_shell = (char *) DEFAULT_SHELL;

	if ((( cur_uid == 0 ) || correct_password ( pw ))) {
		log_su_successful(pw->pw_uid, old_user, tty );
	} else {
		log_su_failure (pw->pw_uid, old_user, tty );
		bb_error_msg_and_die ( "incorrect password" );
	}

#if defined( SYSLOG_SUCCESS ) || defined( SYSLOG_FAILURE )
	closelog();
#endif

	if ( !opt_shell && opt_preserve )
		opt_shell = getenv ( "SHELL" );

	if ( opt_shell && cur_uid && restricted_shell ( pw-> pw_shell )) {
		/* The user being su'd to has a nonstandard shell, and so is
		   probably a uucp account or has restricted access.  Don't
		   compromise the account by allowing access with a standard
		   shell.  */
		fputs ( "using restricted shell\n", stderr );
		opt_shell = 0;
	}

	if ( !opt_shell )
		opt_shell = pw->pw_shell;

	change_identity ( pw );
	setup_environment ( opt_shell, opt_loginshell, !opt_preserve, pw );
	run_shell ( opt_shell, opt_loginshell, opt_command, (const char**)opt_args
#ifdef CONFIG_SELINUX
	, 0
#endif
	);

	return EXIT_FAILURE;
}
