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

#include "pwd.h"
#include "grp.h"

#ifdef CONFIG_FEATURE_SHADOWPASSWDS
#include "shadow.h"
#endif

#include "tinylogin.h"

// import from utmp.c
static void checkutmp(int picky);
static void setutmp(const char *name, const char *line);

// import from encrypt.c
extern char *pw_encrypt(const char *clear, const char *salt);


// login defines
#define DEFAULT_USER  "UNKNOWN"
#define DEFAULT_PWD   "!"
#define DEFAULT_SHELL "/bin/sh"
#define TIMEOUT       60
#define FAIL_DELAY    3
#define MOTD_FILE     "/etc/motd"
#define NOLOGIN_FILE  "/etc/nologin"
#define SECURETTY_FILE "/etc/securetty"

/* Stuff global to this file */
struct utmp utent;


static int check_nologin ( int amroot );

#if defined CONFIG_FEATURE_SECURETTY
static int check_tty ( const char *tty );

#else
static inline int check_tty ( const char *tty )  { return 1; }

#endif

static int is_my_tty ( const char *tty );
static const char *login_prompt ( void );
static void motd ( void );
static void set_env(int argc, char *const *argv);


static void alarm_handler ( int sig )
{
	error_msg ( "\nLogin timed out after %d seconds.\n", TIMEOUT );
	exit ( EXIT_SUCCESS );
}


extern int login_main(int argc, char **argv)
{
	char tty[BUFSIZ];
	char full_tty[200];
	char fromhost[512];
	char *tmp;
	int amroot;
	int flag;
	int failed;
	int count=0;
	struct passwd *pw, pw_copy;

	int opt_preserve = 0;
	int opt_fflag = 0;
	char *opt_host = 0;
	const char *username = 0;
	int alarmstarted = 0;	

	amroot = ( getuid ( ) == 0 );
	signal ( SIGALRM, alarm_handler );
	
	if (( argc > 1 ) && ( TIMEOUT > 0 )) {
		alarm ( TIMEOUT );
		alarmstarted = 1;
	}

	while (( flag = getopt(argc, argv, "f:h:p")) != EOF ) {
		switch ( flag ) {
		case 'p':
			printf ( "PRESERVE\n" );
			opt_preserve = 1;
			break;
		case 'f':
			/*
			 * username must be a seperate token
			 * (-f root, *NOT* -froot). --marekm
			 */
			if ( optarg != argv[optind-1] )
				show_usage ( );

			if ( !amroot ) 		/* Auth bypass only if real UID is zero */
				error_msg_and_die ( "login: -f permission denied\n" );
			
			printf ( "USERNAME: %s\n", optarg );
			
			username = optarg;
			opt_fflag = 1;
			break;
		case 'h':
			opt_host = optarg;
			break;
		default:
			show_usage ( );
		}
	}

	if ( optind < argc )  // got a username
		username = argv [optind++];

	if ( !isatty ( 0 ) || !isatty ( 1 ) || !isatty ( 2 )) 
		return EXIT_FAILURE;		/* Must be a terminal */

	checkutmp ( !amroot );

	tmp = ttyname ( 0 );
	if ( tmp && ( strncmp ( tmp, "/dev/", 5 ) == 0 ))
		safe_strncpy ( tty, tmp + 5, sizeof( tty ));
	else
		safe_strncpy ( tty, "UNKNOWN", sizeof( tty ));

	if ( amroot )
		memset ( utent.ut_host, 0, sizeof utent.ut_host );
	
	if ( opt_host ) {
		safe_strncpy ( utent.ut_host, opt_host, sizeof( utent. ut_host ));
		
		snprintf ( fromhost, sizeof( fromhost ) - 1, " on `%.100s' from `%.200s'", tty, opt_host );
	}
	else
		snprintf ( fromhost, sizeof( fromhost ) - 1, " on `%.100s'", tty );
	
	openlog ( "login", LOG_PID | LOG_CONS | LOG_NOWAIT, LOG_AUTH );

	while ( 1 ) {
		failed = 0;

		if ( !username || !username[0] ) 
			username = login_prompt ( );

		if ( !alarmstarted && ( TIMEOUT > 0 )) {
			alarm ( TIMEOUT );
			alarmstarted = 1;
		}

		if (!( pw = getpwnam ( username ))) {
			pw_copy. pw_name   = DEFAULT_USER;
			pw_copy. pw_passwd = DEFAULT_PWD;
			pw_copy. pw_shell  = DEFAULT_SHELL;
			opt_fflag = 0;
			failed = 1;
		} else 
			pw_copy = *pw;

		pw = &pw_copy;

		if (( pw-> pw_passwd [0] == '!' ) || ( pw-> pw_passwd[0] == '*' ))
			failed = 1;
		
		if ( opt_fflag ) {
			opt_fflag = 0;
			goto auth_ok;
		}

		if (( pw-> pw_uid == 0 ) && ( !check_tty ( tty )))
			failed = 1;

		/* Don't check the password if password entry is empty (!) */
		if ( !pw-> pw_passwd[0] )
			goto auth_ok;

		/* authorization takes place here */
		if ( correct_password ( pw ))
			goto auth_ok;

		syslog ( LOG_WARNING, "invalid password for `%s'%s\n", pw-> pw_name, fromhost);
		failed = 1;
		
auth_ok:
		if ( !failed) 
			break;

		{ // delay next try
			time_t start, now;
			
			time ( &start );
			now = start;
			while ( difftime ( now, start ) < FAIL_DELAY) {
				sleep ( FAIL_DELAY );
				time ( &now );
			}
		}

		puts("Login incorrect");
		if ( ++count == 3 )
			return EXIT_FAILURE;
	}
		
	alarm ( 0 );
	if ( check_nologin ( pw-> pw_uid == 0 ))
		return EXIT_FAILURE;

	setutmp ( username, tty );
	if ( *tty != '/' ) 
		snprintf ( full_tty, sizeof( full_tty ) - 1, "/dev/%s", tty);
	else
		safe_strncpy ( full_tty, tty, sizeof( full_tty ) - 1 );
	
	if ( !is_my_tty ( full_tty ))  
		syslog ( LOG_ERR, "unable to determine TTY name, got %s\n", full_tty );
		
	/* Try these, but don't complain if they fail 
	 * (for example when the root fs is read only) */
	chown ( full_tty, pw-> pw_uid, pw-> pw_gid );
	chmod ( full_tty, 0600 );

	change_identity ( pw );
	setup_environment ( pw-> pw_shell, 1, !opt_preserve, pw );

	motd ( );
	signal ( SIGALRM, SIG_DFL );	/* default alarm signal */

	if ( pw-> pw_uid == 0 ) 
		syslog ( LOG_INFO, "root login %s\n", fromhost );
	
	run_shell ( pw-> pw_shell, 1, 0, 0 );	/* exec the shell finally. */
	
	return EXIT_FAILURE;
}



static const char *login_prompt ( void )
{
	char buf [1024];
	char *sp, *ep;

	gethostname ( buf, sizeof( buf ));
	printf ( "\nBusyBox on %s login: ", buf );
	fflush ( stdout );

	if ( !fgets ( buf, sizeof( buf ) - 1, stdin ))
		return 0;
		
	if ( !strchr ( buf, '\n' ));
		return 0;
	
	for ( sp = buf; isspace ( *sp ); sp++ ) { }
	for ( ep = sp; isgraph ( *ep ); ep++ ) { }

	*ep = 0;		
	return sp;
}


static int check_nologin ( int amroot )
{
	if ( access ( NOLOGIN_FILE, F_OK ) == 0 ) {
		FILE *fp;
		int c;

		if (( fp = fopen ( NOLOGIN_FILE, "r" ))) {
			while (( c = getc ( fp )) != EOF )
				putchar (( c == '\n' ) ? '\r' : c );

			fflush ( stdout );
			fclose ( fp );
		} else {
			puts ( "\r\nSystem closed for routine maintenance.\r" );
		}
		if ( !amroot )
			return 1;
			
		puts ( "\r\n[Disconnect bypassed -- root login allowed.]\r" );
	}
	return 0;
}

#ifdef CONFIG_FEATURE_SECURETTY

static int check_tty ( const char *tty )
{
	FILE *fp;
	int i;
	char buf[BUFSIZ];

	if (( fp = fopen ( SECURETTY_FILE, "r" ))) {
		while ( fgets ( buf, sizeof( buf ) - 1, fp )) {
			for ( i = xstrlen( buf ) - 1; i >= 0; --i ) {
				if ( !isspace ( buf[i] ))
					break;
			}
			buf[++i] = '\0';
			if (( buf [0] == '\0' ) || ( buf [0] == '#' ))
				continue;

			if ( strcmp ( buf, tty ) == 0 ) {
				fclose ( fp );
				return 1;
			}
		}
		fclose(fp);
		return 0;
	}
	else {
		syslog ( LOG_WARNING, "cannot open securetty file.\n" );
		return 1;
	}
}

#endif

/* returns 1 if true */
static int is_my_tty ( const char *tty )
{
	struct stat by_name, by_fd;

	if ( stat ( tty, &by_name ) || fstat ( 0, &by_fd ))
		return 0;
		
	if ( by_name. st_rdev != by_fd. st_rdev )
		return 0;
	else
		return 1;
}


static void motd ( )
{
	FILE *fp;
	register int c;

	if (( fp = fopen ( MOTD_FILE, "r" ))) {
		while (( c = getc ( fp )) != EOF ) 
			putchar ( c );		
		fclose ( fp );
	}
}


// vv  Taken from tinylogin utmp.c  vv

#define _WTMP_FILE "/var/log/wtmp"

#define	NO_UTENT \
	"No utmp entry.  You must exec \"login\" from the lowest level \"sh\""
#define	NO_TTY \
	"Unable to determine your tty name."

/*
 * checkutmp - see if utmp file is correct for this process
 *
 *	System V is very picky about the contents of the utmp file
 *	and requires that a slot for the current process exist.
 *	The utmp file is scanned for an entry with the same process
 *	ID.  If no entry exists the process exits with a message.
 *
 *	The "picky" flag is for network and other logins that may
 *	use special flags.  It allows the pid checks to be overridden.
 *	This means that getty should never invoke login with any
 *	command line flags.
 */

static void checkutmp(int picky)
{
	char *line;
	struct utmp *ut;
	pid_t pid = getpid();

	setutent();

	/* First, try to find a valid utmp entry for this process.  */
	while ((ut = getutent()))
		if (ut->ut_pid == pid && ut->ut_line[0] && ut->ut_id[0] &&
			(ut->ut_type == LOGIN_PROCESS || ut->ut_type == USER_PROCESS))
			break;

	/* If there is one, just use it, otherwise create a new one.  */
	if (ut) {
		utent = *ut;
	} else {
		if (picky) {
			puts(NO_UTENT);
			exit(1);
		}
		line = ttyname(0);
		if (!line) {
			puts(NO_TTY);
			exit(1);
		}
		if (strncmp(line, "/dev/", 5) == 0)
			line += 5;
		memset((void *) &utent, 0, sizeof utent);
		utent.ut_type = LOGIN_PROCESS;
		utent.ut_pid = pid;
		strncpy(utent.ut_line, line, sizeof utent.ut_line);
		/* XXX - assumes /dev/tty?? */
		strncpy(utent.ut_id, utent.ut_line + 3, sizeof utent.ut_id);
		strncpy(utent.ut_user, "LOGIN", sizeof utent.ut_user);
		time(&utent.ut_time);
	}
}

#if __GNU_LIBRARY__ < 5
/*
 * Some systems already have updwtmp() and possibly updwtmpx().  Others
 * don't, so we re-implement these functions if necessary.  --marekm
 */
static void updwtmp(const char *filename, const struct utmp *ut)
{
	int fd;

	fd = open(filename, O_APPEND | O_WRONLY, 0);
	if (fd >= 0) {
		write(fd, (const char *) ut, sizeof(*ut));
		close(fd);
	}
}
#endif

/*
 * setutmp - put a USER_PROCESS entry in the utmp file
 *
 *	setutmp changes the type of the current utmp entry to
 *	USER_PROCESS.  the wtmp file will be updated as well.
 */

static void setutmp(const char *name, const char *line)
{
	utent.ut_type = USER_PROCESS;
	strncpy(utent.ut_user, name, sizeof utent.ut_user);
	time(&utent.ut_time);
	/* other fields already filled in by checkutmp above */
	setutent();
	pututline(&utent);
	endutent();
	updwtmp(_WTMP_FILE, &utent);
}
