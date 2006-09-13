/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include <utmp.h>
#include <sys/resource.h>
#include <syslog.h>

#ifdef CONFIG_SELINUX
#include <selinux/selinux.h>  /* for is_selinux_enabled()  */
#include <selinux/get_context_list.h> /* for get_default_context() */
#include <selinux/flask.h> /* for security class definitions  */
#include <errno.h>
#endif

/* import from utmp.c
 * XXX: FIXME: provide empty bodies if ENABLE_FEATURE_UTMP == 0
 */
static struct utmp utent;
static void read_or_build_utent(int);
static void write_utent(const char *);

enum {
	TIMEOUT = 60,
	EMPTY_USERNAME_COUNT = 10,
	USERNAME_SIZE = 32,
	TTYNAME_SIZE = 32,
};

static void die_if_nologin_and_non_root(int amroot);

#if ENABLE_FEATURE_SECURETTY
static int check_securetty(void);
#else
static inline int check_securetty(void) { return 1; }
#endif

static void get_username_or_die(char *buf, int size_buf);
static void motd(void);

static void nonblock(int fd)
{
	fcntl(fd, F_SETFL, O_NONBLOCK | fcntl(fd, F_GETFL));
}

static void alarm_handler(int sig ATTRIBUTE_UNUSED)
{
	/* This is the escape hatch!  Poor serial line users and the like
	 * arrive here when their connection is broken.
	 * We don't want to block here */
	nonblock(1);
	nonblock(2);
	bb_info_msg("\r\nLogin timed out after %d seconds\r", TIMEOUT);
	exit(EXIT_SUCCESS);
}


static char full_tty[TTYNAME_SIZE];
static char* short_tty = full_tty;


int login_main(int argc, char **argv)
{
	char fromhost[512];
	char username[USERNAME_SIZE];
	const char *tmp;
	int amroot;
	int flag;
	int count = 0;
	struct passwd *pw;
	int opt_preserve = 0;
	int opt_fflag = 0;
	char *opt_host = 0;
#ifdef CONFIG_SELINUX
	security_context_t user_sid = NULL;
#endif

	username[0] = '\0';
	amroot = (getuid() == 0);
	signal(SIGALRM, alarm_handler);
	alarm(TIMEOUT);

	while ((flag = getopt(argc, argv, "f:h:p")) != EOF) {
		switch (flag) {
		case 'p':
			opt_preserve = 1;
			break;
		case 'f':
			/*
			 * username must be a separate token
			 * (-f root, *NOT* -froot). --marekm
			 */
			if (optarg != argv[optind-1])
				bb_show_usage();

			if (!amroot)	/* Auth bypass only if real UID is zero */
				bb_error_msg_and_die("-f is for root only");

			safe_strncpy(username, optarg, sizeof(username));
			opt_fflag = 1;
			break;
		case 'h':
			opt_host = optarg;
			break;
		default:
			bb_show_usage();
		}
	}
	if (optind < argc)             /* user from command line (getty) */
		safe_strncpy(username, argv[optind], sizeof(username));

	/* Let's find out and memorize our tty */
	if (!isatty(0) || !isatty(1) || !isatty(2))
		return EXIT_FAILURE;		/* Must be a terminal */
	safe_strncpy(full_tty, "UNKNOWN", sizeof(full_tty));
	tmp = ttyname(0);
	if (tmp) {
		safe_strncpy(full_tty, tmp, sizeof(full_tty));
		if (strncmp(full_tty, "/dev/", 5) == 0)
			short_tty = full_tty + 5;
	}

	if (ENABLE_FEATURE_UTMP) {
		read_or_build_utent(!amroot);
		if (amroot)
			memset(utent.ut_host, 0, sizeof(utent.ut_host));
	}

	if (opt_host) {
		if (ENABLE_FEATURE_UTMP)
			safe_strncpy(utent.ut_host, opt_host, sizeof(utent.ut_host));
		snprintf(fromhost, sizeof(fromhost)-1, " on `%.100s' from "
					"`%.200s'", short_tty, opt_host);
	}
	else
		snprintf(fromhost, sizeof(fromhost)-1, " on `%.100s'", short_tty);

	bb_setpgrp;

	openlog(bb_applet_name, LOG_PID | LOG_CONS | LOG_NOWAIT, LOG_AUTH);

	while (1) {
		if (!username[0])
			get_username_or_die(username, sizeof(username));

		pw = getpwnam(username);
		if (!pw) {
			safe_strncpy(username, "UNKNOWN", sizeof(username));
			goto auth_failed;
		}

		if (pw->pw_passwd[0] == '!' || pw->pw_passwd[0] == '*')
			goto auth_failed;

		if (opt_fflag)
			break; /* -f USER: success without asking passwd */

		if (pw->pw_uid == 0 && !check_securetty())
			goto auth_failed;

		/* Don't check the password if password entry is empty (!) */
		if (!pw->pw_passwd[0])
			break; 

		/* authorization takes place here */
		if (correct_password(pw))
			break; 

auth_failed:
		opt_fflag = 0;
		bb_do_delay(FAIL_DELAY);
		puts("Login incorrect");
		if (++count == 3) {
			syslog(LOG_WARNING, "invalid password for `%s'%s",
						username, fromhost);
			return EXIT_FAILURE;
		}
		username[0] = '\0';
	}

	alarm(0);
	die_if_nologin_and_non_root(pw->pw_uid == 0);

	if (ENABLE_FEATURE_UTMP)
		write_utent(username);

#ifdef CONFIG_SELINUX
	if (is_selinux_enabled()) {
		security_context_t old_tty_sid, new_tty_sid;

		if (get_default_context(username, NULL, &user_sid)) {
			bb_error_msg_and_die("unable to get SID for %s",
					username);
		}
		if (getfilecon(full_tty, &old_tty_sid) < 0) {
			bb_perror_msg_and_die("getfilecon(%.100s) failed",
					full_tty);
		}
		if (security_compute_relabel(user_sid, old_tty_sid,
					SECCLASS_CHR_FILE, &new_tty_sid) != 0) {
			bb_perror_msg_and_die("security_change_sid(%.100s) failed",
					full_tty);
		}
		if (setfilecon(full_tty, new_tty_sid) != 0) {
			bb_perror_msg_and_die("chsid(%.100s, %s) failed",
					full_tty, new_tty_sid);
		}
	}
#endif
	/* Try these, but don't complain if they fail.
	 * _f_chown is safe wrt race t=ttyname(0);...;chown(t); */
	fchown(0, pw->pw_uid, pw->pw_gid);
	fchmod(0, 0600);

	if (ENABLE_LOGIN_SCRIPTS) {
		char *script = getenv("LOGIN_PRE_SUID_SCRIPT");
		if (script) {
			char *t_argv[2] = { script, NULL };
			switch (fork()) {
			case -1: break;
			case 0: /* child */
				xchdir("/");
				setenv("LOGIN_TTY", full_tty, 1);
				setenv("LOGIN_USER", pw->pw_name, 1);
				setenv("LOGIN_UID", utoa(pw->pw_uid), 1);
				setenv("LOGIN_GID", utoa(pw->pw_gid), 1);
				setenv("LOGIN_SHELL", pw->pw_shell, 1);
				execvp(script, t_argv);
				exit(1);
			default: /* parent */
				wait(NULL);
			}
		}
	}

	change_identity(pw);
	tmp = pw->pw_shell;
	if (!tmp || !*tmp)
		tmp = DEFAULT_SHELL;
	setup_environment(tmp, 1, !opt_preserve, pw);

	motd();
	signal(SIGALRM, SIG_DFL);	/* default alarm signal */

	if (pw->pw_uid == 0)
		syslog(LOG_INFO, "root login%s", fromhost);
#ifdef CONFIG_SELINUX
	/* well, a simple setexeccon() here would do the job as well,
	 * but let's play the game for now */
	set_current_security_context(user_sid);
#endif
	run_shell(tmp, 1, 0, 0);	/* exec the shell finally. */

	return EXIT_FAILURE;
}


static void get_username_or_die(char *buf, int size_buf)
{
	int c, cntdown;
	cntdown = EMPTY_USERNAME_COUNT;
prompt:
	/* skip whitespace */
	print_login_prompt();
	do {
		c = getchar();
		if (c == EOF) exit(1);
		if (c == '\n') {
			if (!--cntdown) exit(1);
			goto prompt;
		}
	} while (isspace(c));

	*buf++ = c;
	if (!fgets(buf, size_buf-2, stdin))
		exit(1);
	if (!strchr(buf, '\n'))
		exit(1);
	while (isgraph(*buf)) buf++;
	*buf = '\0';
}


static void die_if_nologin_and_non_root(int amroot)
{
	FILE *fp;
	int c;

	if (access(bb_path_nologin_file, F_OK))
		return;

	fp = fopen(bb_path_nologin_file, "r");
	if (fp) {
		while ((c = getc(fp)) != EOF)
			putchar((c=='\n') ? '\r' : c);
		fflush(stdout);
		fclose(fp);
	} else
		puts("\r\nSystem closed for routine maintenance\r");
	if (!amroot)
		exit(1);
	puts("\r\n[Disconnect bypassed -- root login allowed.]\r");
}

#if ENABLE_FEATURE_SECURETTY

static int check_securetty(void)
{
	FILE *fp;
	int i;
	char buf[BUFSIZ];

	fp = fopen(bb_path_securetty_file, "r");
	if (!fp) {
		/* A missing securetty file is not an error. */
		return 1;
	}
	while (fgets(buf, sizeof(buf)-1, fp)) {
		for(i = strlen(buf)-1; i>=0; --i) {
			if (!isspace(buf[i]))
				break;
		}
		buf[++i] = '\0';
		if ((buf[0]=='\0') || (buf[0]=='#'))
			continue;
		if (strcmp(buf, short_tty) == 0) {
			fclose(fp);
			return 1;
		}
	}
	fclose(fp);
	return 0;
}

#endif

static void motd(void)
{
	FILE *fp;
	int c;

	fp = fopen(bb_path_motd_file, "r");
	if (fp) {
		while ((c = getc(fp)) != EOF)
			putchar(c);
		fclose(fp);
	}
}


#if ENABLE_FEATURE_UTMP
/* vv  Taken from tinylogin utmp.c  vv */

/*
 * read_or_build_utent - see if utmp file is correct for this process
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

static void read_or_build_utent(int picky)
{
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
		if (picky)
			bb_error_msg_and_die("no utmp entry found");

		memset(&utent, 0, sizeof(utent));
		utent.ut_type = LOGIN_PROCESS;
		utent.ut_pid = pid;
		strncpy(utent.ut_line, short_tty, sizeof(utent.ut_line));
		/* This one is only 4 chars wide. Try to fit something
		 * remotely meaningful by skipping "tty"... */
		strncpy(utent.ut_id, short_tty + 3, sizeof(utent.ut_id));
		strncpy(utent.ut_user, "LOGIN", sizeof(utent.ut_user));
		utent.ut_time = time(NULL);
	}
}

/*
 * write_utent - put a USER_PROCESS entry in the utmp file
 *
 *	write_utent changes the type of the current utmp entry to
 *	USER_PROCESS.  the wtmp file will be updated as well.
 */

static void write_utent(const char *username)
{
	utent.ut_type = USER_PROCESS;
	strncpy(utent.ut_user, username, sizeof(utent.ut_user));
	utent.ut_time = time(NULL);
	/* other fields already filled in by read_or_build_utent above */
	setutent();
	pututline(&utent);
	endutent();
#if ENABLE_FEATURE_WTMP
	if (access(bb_path_wtmp_file, R_OK|W_OK) == -1) {
		close(creat(bb_path_wtmp_file, 0664));
	}
	updwtmp(bb_path_wtmp_file, &utent);
#endif
}
#endif /* CONFIG_FEATURE_UTMP */
