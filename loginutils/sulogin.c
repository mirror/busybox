/* vi: set sw=4 ts=4: */
/*
 * Mini sulogin implementation for busybox
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <syslog.h>

#include "busybox.h"


#define SULOGIN_PROMPT "Give root password for system maintenance\n" \
	"(or type Control-D for normal startup):"

static const char * const forbid[] = {
	"ENV",
	"BASH_ENV",
	"HOME",
	"IFS",
	"PATH",
	"SHELL",
	"LD_LIBRARY_PATH",
	"LD_PRELOAD",
	"LD_TRACE_LOADED_OBJECTS",
	"LD_BIND_NOW",
	"LD_AOUT_LIBRARY_PATH",
	"LD_AOUT_PRELOAD",
	"LD_NOWARN",
	"LD_KEEPDIR",
	(char *) 0
};



static void catchalarm(int ATTRIBUTE_UNUSED junk)
{
	exit(EXIT_FAILURE);
}


int sulogin_main(int argc, char **argv)
{
	char *cp;
	int timeout = 0;
	char *timeout_arg;
	const char * const *p;
	struct passwd *pwd;
	struct spwd *spwd;

	if (ENABLE_FEATURE_SYSLOG) {
		logmode = LOGMODE_BOTH;
		openlog(bb_applet_name, LOG_CONS | LOG_NOWAIT, LOG_AUTH);
	}

	if (bb_getopt_ulflags (argc, argv, "t:", &timeout_arg)) {
		if (safe_strtoi(timeout_arg, &timeout)) {
			timeout = 0;
		}
	}

	if (argv[optind]) {
		close(0);
		close(1);
		close(2);
		dup(xopen(argv[optind], O_RDWR));
		dup(0);
	}

	if (!isatty(0) || !isatty(1) || !isatty(2)) {
		bb_error_msg_and_die("Not a tty");
	}

	/* Clear out anything dangerous from the environment */
	for (p = forbid; *p; p++)
		unsetenv(*p);

	signal(SIGALRM, catchalarm);

	if (!(pwd = getpwuid(0))) {
		goto AUTH_ERROR;
	} 

	if (ENABLE_FEATURE_SHADOWPASSWDS) {
		if (!(spwd = getspnam(pwd->pw_name))) {
			goto AUTH_ERROR;
		}
		pwd->pw_passwd = spwd->sp_pwdp;
	}

	while (1) {
		/* cp points to a static buffer that is zeroed every time */
		cp = bb_askpass(timeout, SULOGIN_PROMPT);
		if (!cp || !*cp) {
			bb_info_msg("Normal startup");
			exit(EXIT_SUCCESS);
		}
		if (strcmp(pw_encrypt(cp, pwd->pw_passwd), pwd->pw_passwd) == 0) {
			break;
		}
		bb_do_delay(FAIL_DELAY);
		bb_error_msg("Login incorrect");
	}
	memset(cp, 0, strlen(cp));
	signal(SIGALRM, SIG_DFL);

	bb_info_msg("System Maintenance Mode");

	USE_SELINUX(renew_current_security_context());

	run_shell(pwd->pw_shell, 1, 0, 0);
	/* never returns */
AUTH_ERROR:	
	bb_error_msg_and_die("No password entry for `root'");
}
