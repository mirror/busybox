/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include <syslog.h>
#include <utmp.h>
#include <sys/resource.h>

#if ENABLE_SELINUX
#include <selinux/selinux.h>  /* for is_selinux_enabled()  */
#include <selinux/get_context_list.h> /* for get_default_context() */
#include <selinux/flask.h> /* for security class definitions  */
#endif

#if ENABLE_PAM
/* PAM may include <locale.h>. We may need to undefine bbox's stub define: */
#undef setlocale
/* For some obscure reason, PAM is not in pam/xxx, but in security/xxx.
 * Apparently they like to confuse people. */
#include <security/pam_appl.h>
#include <security/pam_misc.h>
static const struct pam_conv conv = {
	misc_conv,
	NULL
};
#endif

enum {
	TIMEOUT = 60,
	EMPTY_USERNAME_COUNT = 10,
	USERNAME_SIZE = 32,
	TTYNAME_SIZE = 32,
};

static char* short_tty;

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

static void read_or_build_utent(struct utmp *utptr, int picky)
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
		*utptr = *ut;
	} else {
		if (picky)
			bb_error_msg_and_die("no utmp entry found");

		memset(utptr, 0, sizeof(*utptr));
		utptr->ut_type = LOGIN_PROCESS;
		utptr->ut_pid = pid;
		strncpy(utptr->ut_line, short_tty, sizeof(utptr->ut_line));
		/* This one is only 4 chars wide. Try to fit something
		 * remotely meaningful by skipping "tty"... */
		strncpy(utptr->ut_id, short_tty + 3, sizeof(utptr->ut_id));
		strncpy(utptr->ut_user, "LOGIN", sizeof(utptr->ut_user));
		utptr->ut_tv.tv_sec = time(NULL);
	}
	if (!picky)	/* root login */
		memset(utptr->ut_host, 0, sizeof(utptr->ut_host));
}

/*
 * write_utent - put a USER_PROCESS entry in the utmp file
 *
 *	write_utent changes the type of the current utmp entry to
 *	USER_PROCESS.  the wtmp file will be updated as well.
 */
static void write_utent(struct utmp *utptr, const char *username)
{
	utptr->ut_type = USER_PROCESS;
	strncpy(utptr->ut_user, username, sizeof(utptr->ut_user));
	utptr->ut_tv.tv_sec = time(NULL);
	/* other fields already filled in by read_or_build_utent above */
	setutent();
	pututline(utptr);
	endutent();
#if ENABLE_FEATURE_WTMP
	if (access(bb_path_wtmp_file, R_OK|W_OK) == -1) {
		close(creat(bb_path_wtmp_file, 0664));
	}
	updwtmp(bb_path_wtmp_file, utptr);
#endif
}
#else /* !ENABLE_FEATURE_UTMP */
#define read_or_build_utent(utptr, picky) ((void)0)
#define write_utent(utptr, username) ((void)0)
#endif /* !ENABLE_FEATURE_UTMP */

#if ENABLE_FEATURE_NOLOGIN
static void die_if_nologin(void)
{
	FILE *fp;
	int c;

	if (access("/etc/nologin", F_OK))
		return;

	fp = fopen("/etc/nologin", "r");
	if (fp) {
		while ((c = getc(fp)) != EOF)
			bb_putchar((c=='\n') ? '\r' : c);
		fflush(stdout);
		fclose(fp);
	} else
		puts("\r\nSystem closed for routine maintenance\r");
	exit(EXIT_FAILURE);
}
#else
static ALWAYS_INLINE void die_if_nologin(void) {}
#endif

#if ENABLE_FEATURE_SECURETTY && !ENABLE_PAM
static int check_securetty(void)
{
	FILE *fp;
	int i;
	char buf[256];

	fp = fopen("/etc/securetty", "r");
	if (!fp) {
		/* A missing securetty file is not an error. */
		return 1;
	}
	while (fgets(buf, sizeof(buf)-1, fp)) {
		for (i = strlen(buf)-1; i >= 0; --i) {
			if (!isspace(buf[i]))
				break;
		}
		buf[++i] = '\0';
		if (!buf[0] || (buf[0] == '#'))
			continue;
		if (strcmp(buf, short_tty) == 0) {
			fclose(fp);
			return 1;
		}
	}
	fclose(fp);
	return 0;
}
#else
static ALWAYS_INLINE int check_securetty(void) { return 1; }
#endif

static void get_username_or_die(char *buf, int size_buf)
{
	int c, cntdown;

	cntdown = EMPTY_USERNAME_COUNT;
 prompt:
	print_login_prompt();
	/* skip whitespace */
	do {
		c = getchar();
		if (c == EOF) exit(EXIT_FAILURE);
		if (c == '\n') {
			if (!--cntdown) exit(EXIT_FAILURE);
			goto prompt;
		}
	} while (isspace(c));

	*buf++ = c;
	if (!fgets(buf, size_buf-2, stdin))
		exit(EXIT_FAILURE);
	if (!strchr(buf, '\n'))
		exit(EXIT_FAILURE);
	while (isgraph(*buf)) buf++;
	*buf = '\0';
}

static void motd(void)
{
	int fd;

	fd = open(bb_path_motd_file, O_RDONLY);
	if (fd >= 0) {
		fflush(stdout);
		bb_copyfd_eof(fd, STDOUT_FILENO);
		close(fd);
	}
}

static void alarm_handler(int sig ATTRIBUTE_UNUSED)
{
	/* This is the escape hatch!  Poor serial line users and the like
	 * arrive here when their connection is broken.
	 * We don't want to block here */
	ndelay_on(1);
	printf("\r\nLogin timed out after %d seconds\r\n", TIMEOUT);
	fflush(stdout);
	/* unix API is brain damaged regarding O_NONBLOCK,
	 * we should undo it, or else we can affect other processes */
	ndelay_off(1);
	_exit(EXIT_SUCCESS);
}

int login_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int login_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	enum {
		LOGIN_OPT_f = (1<<0),
		LOGIN_OPT_h = (1<<1),
		LOGIN_OPT_p = (1<<2),
	};
	char *fromhost;
	char username[USERNAME_SIZE];
	const char *tmp;
	int amroot;
	unsigned opt;
	int count = 0;
	struct passwd *pw;
	char *opt_host = opt_host; /* for compiler */
	char *opt_user = opt_user; /* for compiler */
	char full_tty[TTYNAME_SIZE];
	USE_SELINUX(security_context_t user_sid = NULL;)
	USE_FEATURE_UTMP(struct utmp utent;)
#if ENABLE_PAM
	int pamret;
	pam_handle_t *pamh;
	const char *pamuser;
	const char *failed_msg;
	struct passwd pwdstruct;
	char pwdbuf[256];
#endif

	short_tty = full_tty;
	username[0] = '\0';
	signal(SIGALRM, alarm_handler);
	alarm(TIMEOUT);

	/* More of suid paranoia if called by non-root */
	amroot = !sanitize_env_if_suid(); /* Clear dangerous stuff, set PATH */

	/* Mandatory paranoia for suid applet:
	 * ensure that fd# 0,1,2 are opened (at least to /dev/null)
	 * and any extra open fd's are closed.
	 * (The name of the function is misleading. Not daemonizing here.) */
	bb_daemonize_or_rexec(DAEMON_ONLY_SANITIZE | DAEMON_CLOSE_EXTRA_FDS, NULL);

	opt = getopt32(argv, "f:h:p", &opt_user, &opt_host);
	if (opt & LOGIN_OPT_f) {
		if (!amroot)
			bb_error_msg_and_die("-f is for root only");
		safe_strncpy(username, opt_user, sizeof(username));
	}
	argv += optind;
	if (argv[0]) /* user from command line (getty) */
		safe_strncpy(username, argv[0], sizeof(username));

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

	read_or_build_utent(&utent, !amroot);

	if (opt & LOGIN_OPT_h) {
		USE_FEATURE_UTMP(
			safe_strncpy(utent.ut_host, opt_host, sizeof(utent.ut_host));
		)
		fromhost = xasprintf(" on '%s' from '%s'", short_tty, opt_host);
	} else
		fromhost = xasprintf(" on '%s'", short_tty);

	/* Was breaking "login <username>" from shell command line: */
	/*bb_setpgrp();*/

	openlog(applet_name, LOG_PID | LOG_CONS | LOG_NOWAIT, LOG_AUTH);

	while (1) {
		/* flush away any type-ahead (as getty does) */
		ioctl(0, TCFLSH, TCIFLUSH);

		if (!username[0])
			get_username_or_die(username, sizeof(username));

#if ENABLE_PAM
		pamret = pam_start("login", username, &conv, &pamh);
		if (pamret != PAM_SUCCESS) {
			failed_msg = "start";
			goto pam_auth_failed;
		}
		/* set TTY (so things like securetty work) */
		pamret = pam_set_item(pamh, PAM_TTY, short_tty);
		if (pamret != PAM_SUCCESS) {
			failed_msg = "set_item(TTY)";
			goto pam_auth_failed;
		}
		pamret = pam_authenticate(pamh, 0);
		if (pamret != PAM_SUCCESS) {
			failed_msg = "authenticate";
			goto pam_auth_failed;
			/* TODO: or just "goto auth_failed"
			 * since user seems to enter wrong password
			 * (in this case pamret == 7)
			 */
		}
		/* check that the account is healthy */
		pamret = pam_acct_mgmt(pamh, 0);
		if (pamret != PAM_SUCCESS) {
			failed_msg = "acct_mgmt";
			goto pam_auth_failed;
		}
		/* read user back */
		pamuser = NULL;
		/* gcc: "dereferencing type-punned pointer breaks aliasing rules..."
		 * thus we cast to (void*) */
		if (pam_get_item(pamh, PAM_USER, (void*)&pamuser) != PAM_SUCCESS) {
			failed_msg = "get_item(USER)";
			goto pam_auth_failed;
		}
		if (!pamuser || !pamuser[0])
			goto auth_failed;
		safe_strncpy(username, pamuser, sizeof(username));
		/* Don't use "pw = getpwnam(username);",
		 * PAM is said to be capable of destroying static storage
		 * used by getpwnam(). We are using safe(r) function */
		pw = NULL;
		getpwnam_r(username, &pwdstruct, pwdbuf, sizeof(pwdbuf), &pw);
		if (!pw)
			goto auth_failed;
		pamret = pam_open_session(pamh, 0);
		if (pamret != PAM_SUCCESS) {
			failed_msg = "open_session";
			goto pam_auth_failed;
		}
		pamret = pam_setcred(pamh, PAM_ESTABLISH_CRED);
		if (pamret != PAM_SUCCESS) {
			failed_msg = "setcred";
			goto pam_auth_failed;
		}
		break; /* success, continue login process */

 pam_auth_failed:
		bb_error_msg("pam_%s call failed: %s (%d)", failed_msg,
					pam_strerror(pamh, pamret), pamret);
		safe_strncpy(username, "UNKNOWN", sizeof(username));
#else /* not PAM */
		pw = getpwnam(username);
		if (!pw) {
			strcpy(username, "UNKNOWN");
			goto fake_it;
		}

		if (pw->pw_passwd[0] == '!' || pw->pw_passwd[0] == '*')
			goto auth_failed;

		if (opt & LOGIN_OPT_f)
			break; /* -f USER: success without asking passwd */

		if (pw->pw_uid == 0 && !check_securetty())
			goto auth_failed;

		/* Don't check the password if password entry is empty (!) */
		if (!pw->pw_passwd[0])
			break;
 fake_it:
		/* authorization takes place here */
		if (correct_password(pw))
			break;
#endif /* ENABLE_PAM */
 auth_failed:
		opt &= ~LOGIN_OPT_f;
		bb_do_delay(FAIL_DELAY);
		/* TODO: doesn't sound like correct English phrase to me */
		puts("Login incorrect");
		if (++count == 3) {
			syslog(LOG_WARNING, "invalid password for '%s'%s",
						username, fromhost);
			return EXIT_FAILURE;
		}
		username[0] = '\0';
	}

	alarm(0);
	if (!amroot)
		die_if_nologin();

	write_utent(&utent, username);

#if ENABLE_SELINUX
	if (is_selinux_enabled()) {
		security_context_t old_tty_sid, new_tty_sid;

		if (get_default_context(username, NULL, &user_sid)) {
			bb_error_msg_and_die("cannot get SID for %s",
					username);
		}
		if (getfilecon(full_tty, &old_tty_sid) < 0) {
			bb_perror_msg_and_die("getfilecon(%s) failed",
					full_tty);
		}
		if (security_compute_relabel(user_sid, old_tty_sid,
					SECCLASS_CHR_FILE, &new_tty_sid) != 0) {
			bb_perror_msg_and_die("security_change_sid(%s) failed",
					full_tty);
		}
		if (setfilecon(full_tty, new_tty_sid) != 0) {
			bb_perror_msg_and_die("chsid(%s, %s) failed",
					full_tty, new_tty_sid);
		}
	}
#endif
	/* Try these, but don't complain if they fail.
	 * _f_chown is safe wrt race t=ttyname(0);...;chown(t); */
	fchown(0, pw->pw_uid, pw->pw_gid);
	fchmod(0, 0600);

	/* We trust environment only if we run by root */
	if (ENABLE_LOGIN_SCRIPTS && amroot) {
		char *t_argv[2];

		t_argv[0] = getenv("LOGIN_PRE_SUID_SCRIPT");
		if (t_argv[0]) {
			t_argv[1] = NULL;
			xsetenv("LOGIN_TTY", full_tty);
			xsetenv("LOGIN_USER", pw->pw_name);
			xsetenv("LOGIN_UID", utoa(pw->pw_uid));
			xsetenv("LOGIN_GID", utoa(pw->pw_gid));
			xsetenv("LOGIN_SHELL", pw->pw_shell);
			spawn_and_wait(t_argv); /* NOMMU-friendly */
			unsetenv("LOGIN_TTY"  );
			unsetenv("LOGIN_USER" );
			unsetenv("LOGIN_UID"  );
			unsetenv("LOGIN_GID"  );
			unsetenv("LOGIN_SHELL");
		}
	}

	change_identity(pw);
	tmp = pw->pw_shell;
	if (!tmp || !*tmp)
		tmp = DEFAULT_SHELL;
	/* setup_environment params: shell, clear_env, change_env, pw */
	setup_environment(tmp, !(opt & LOGIN_OPT_p), 1, pw);

	motd();

	if (pw->pw_uid == 0)
		syslog(LOG_INFO, "root login%s", fromhost);
#if ENABLE_SELINUX
	/* well, a simple setexeccon() here would do the job as well,
	 * but let's play the game for now */
	set_current_security_context(user_sid);
#endif

	// util-linux login also does:
	// /* start new session */
	// setsid();
	// /* TIOCSCTTY: steal tty from other process group */
	// if (ioctl(0, TIOCSCTTY, 1)) error_msg...
	// BBox login used to do this (see above):
	// bb_setpgrp();
	// If this stuff is really needed, add it and explain why!

	/* set signals to defaults */
	signal(SIGALRM, SIG_DFL);
	/* Is this correct? This way user can ctrl-c out of /etc/profile,
	 * potentially creating security breach (tested with bash 3.0).
	 * But without this, bash 3.0 will not enable ctrl-c either.
	 * Maybe bash is buggy?
	 * Need to find out what standards say about /bin/login -
	 * should it leave SIGINT etc enabled or disabled? */
	signal(SIGINT, SIG_DFL);

	/* Exec login shell with no additional parameters */
	run_shell(tmp, 1, NULL, NULL);

	/* return EXIT_FAILURE; - not reached */
}
