/* vi: set sw=4 ts=4: */
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
#include <sys/time.h>
#include <sys/types.h>
#include <ctype.h>
#include <time.h>

#include "busybox.h"


// sulogin defines
#define SULOGIN_PROMPT "\nGive root password for system maintenance\n" \
	"(or type Control-D for normal startup):"

static const char *forbid[] = {
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



static void catchalarm(int junk)
{
	exit(EXIT_FAILURE);
}


extern int sulogin_main(int argc, char **argv)
{
	char *cp;
	char *device = (char *) 0;
	const char *name = "root";
	int timeout = 0;
	static char pass[BUFSIZ];
	struct passwd pwent;
	struct passwd *pwd;
	time_t start, now;
	const char **p;
#ifdef CONFIG_FEATURE_SHADOWPASSWDS
	struct spwd *spwd = NULL;
#endif							/* CONFIG_FEATURE_SHADOWPASSWDS */

	openlog("sulogin", LOG_PID | LOG_CONS | LOG_NOWAIT, LOG_AUTH);
	if (argc > 1) {
		if (strncmp(argv[1], "-t", 2) == 0) {
			if (strcmp(argv[1], "-t") == 0) {
				if (argc > 2) {
					timeout = atoi(argv[2]);
					if (argc > 3) {
						device = argv[3];
					}
				}
			} else {
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
			if (open(device, O_RDWR) >= 0) {
				dup(0);
				dup(0);
			} else {
				syslog(LOG_WARNING, "cannot open %s\n", device);
				exit(EXIT_FAILURE);
			}
		}
	}
	if (access(bb_path_passwd_file, 0) == -1) {
		syslog(LOG_WARNING, "No password file\n");
		bb_error_msg_and_die("No password file\n");
	}
	if (!isatty(0) || !isatty(1) || !isatty(2)) {
		exit(EXIT_FAILURE);
	}


	/* Clear out anything dangerous from the environment */
	for (p = forbid; *p; p++)
		unsetenv(*p);


	signal(SIGALRM, catchalarm);
	if (!(pwd = getpwnam(name))) {
		syslog(LOG_WARNING, "No password entry for `root'\n");
		bb_error_msg_and_die("No password entry for `root'\n");
	}
	pwent = *pwd;
#ifdef CONFIG_FEATURE_SHADOWPASSWDS
	spwd = NULL;
	if (pwd && ((strcmp(pwd->pw_passwd, "x") == 0)
				|| (strcmp(pwd->pw_passwd, "*") == 0))) {
		endspent();
		spwd = getspnam(name);
		if (spwd) {
			pwent.pw_passwd = spwd->sp_pwdp;
		}
	}
#endif							/* CONFIG_FEATURE_SHADOWPASSWDS */
	while (1) {
		cp = bb_askpass(timeout, SULOGIN_PROMPT);
		if (!cp || !*cp) {
			puts("\n");
			fflush(stdout);
			syslog(LOG_INFO, "Normal startup\n");
			exit(EXIT_SUCCESS);
		} else {
			safe_strncpy(pass, cp, sizeof(pass));
			bzero(cp, strlen(cp));
		}
		if (strcmp(pw_encrypt(pass, pwent.pw_passwd), pwent.pw_passwd) == 0) {
			break;
		}
		time(&start);
		now = start;
		while (difftime(now, start) < FAIL_DELAY) {
			sleep(FAIL_DELAY);
			time(&now);
		}
		puts("Login incorrect");
		fflush(stdout);
		syslog(LOG_WARNING, "Incorrect root password\n");
	}
	bzero(pass, strlen(pass));
	signal(SIGALRM, SIG_DFL);
	puts("Entering System Maintenance Mode\n");
	fflush(stdout);
	syslog(LOG_INFO, "System Maintenance Mode\n");
	run_shell(pwent.pw_shell, 1, 0, 0);
	return (0);
}
