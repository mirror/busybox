/* vi: set sw=4 ts=4: */
/*
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <utmp.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>
#include <time.h>

#include "busybox.h"


#define SULOGIN_PROMPT "\nGive root password for system maintenance\n" \
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
	char *device = NULL;
	const char *name = "root";
	int timeout = 0;

#define pass bb_common_bufsiz1

	struct passwd pwent;
	struct passwd *pwd;
	const char * const *p;
#if ENABLE_FEATURE_SHADOWPASSWDS
	struct spwd *spwd = NULL;
#endif

	openlog("sulogin", LOG_PID | LOG_NOWAIT, LOG_AUTH);
	logmode = LOGMODE_BOTH;
	if (argc > 1) {
		if (strncmp(argv[1], "-t", 2) == 0) {
			if (argv[1][2] == '\0') { /* -t NN */
				if (argc > 2) {
					timeout = atoi(argv[2]);
					if (argc > 3) {
						device = argv[3];
					}
				}
			} else { /* -tNNN */
				timeout = atoi(&argv[1][2]);
				if (argc > 2) {
					device = argv[2];
				}
			}
		} else {
			device = argv[1];
		}
		if (device) {
			close(0);
			close(1);
			close(2);
			if (open(device, O_RDWR) == 0) {
				dup(0);
				dup(0);
			} else {
				/* Well, it will go only to syslog :) */
				bb_perror_msg_and_die("Cannot open %s", device);
			}
		}
	}
	if (!isatty(0) || !isatty(1) || !isatty(2)) {
		exit(EXIT_FAILURE);
	}
	if (access(bb_path_passwd_file, 0) == -1) {
		bb_error_msg_and_die("No password file");
	}

	/* Clear out anything dangerous from the environment */
	for (p = forbid; *p; p++)
		unsetenv(*p);

	signal(SIGALRM, catchalarm);
	if (!(pwd = getpwnam(name))) {
		bb_error_msg_and_die("No password entry for `root'");
	}
	pwent = *pwd;
#if ENABLE_FEATURE_SHADOWPASSWDS
	spwd = NULL;
	if (pwd && ((strcmp(pwd->pw_passwd, "x") == 0)
				|| (strcmp(pwd->pw_passwd, "*") == 0))) {
		endspent();
		spwd = getspnam(name);
		if (spwd) {
			pwent.pw_passwd = spwd->sp_pwdp;
		}
	}
#endif
	while (1) {
		cp = bb_askpass(timeout, SULOGIN_PROMPT);
		if (!cp || !*cp) {
			puts("\n"); /* Why only on error path? */
			fflush(stdout);
			/* Why only to syslog? */
			syslog(LOG_INFO, "Normal startup");
			exit(EXIT_SUCCESS);
		} else {
			safe_strncpy(pass, cp, sizeof(pass));
			memset(cp, 0, strlen(cp));
		}
		if (strcmp(pw_encrypt(pass, pwent.pw_passwd), pwent.pw_passwd) == 0) {
			break;
		}
		bb_do_delay(FAIL_DELAY);
		bb_error_msg("Incorrect root password");
	}
	memset(pass, 0, strlen(pass));
	signal(SIGALRM, SIG_DFL);
	bb_info_msg("Entering System Maintenance Mode");

#if ENABLE_SELINUX
	renew_current_security_context();
#endif

	run_shell(pwent.pw_shell, 1, 0, 0);

	return 0;
}
