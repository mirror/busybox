/* vi: set sw=4 ts=4: */
/*
 * Mini klogd implementation for busybox
 *
 * Copyright (C) 2001 by Gennady Feldman <gfeldman@gena01.com>.
 * Changes: Made this a standalone busybox module which uses standalone
 *					syslog() client interface.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Copyright (C) 2000 by Karl M. Hegbloom <karlheg@debian.org>
 *
 * "circular buffer" Copyright (C) 2000 by Gennady Feldman <gfeldman@gena01.com>
 *
 * Maintainer: Gennady Feldman <gfeldman@gena01.com> as of Mar 12, 2001
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>		/* for our signal() handlers */
#include <string.h>		/* strncpy() */
#include <errno.h>		/* errno and friends */
#include <unistd.h>
#include <ctype.h>
#include <sys/syslog.h>
#include <sys/klog.h>

#include "busybox.h"

static void klogd_signal(int sig)
{
	klogctl(7, NULL, 0);
	klogctl(0, 0, 0);
	/* logMessage(0, "Kernel log daemon exiting."); */
	syslog(LOG_NOTICE, "Kernel log daemon exiting.");
	exit(EXIT_SUCCESS);
}

static void doKlogd(const int console_log_level) ATTRIBUTE_NORETURN;
static void doKlogd(const int console_log_level)
{
	int priority = LOG_INFO;
	char log_buffer[4096];
	int i, n, lastc;
	char *start;

	openlog("kernel", 0, LOG_KERN);

	/* Set up sig handlers */
	signal(SIGINT, klogd_signal);
	signal(SIGKILL, klogd_signal);
	signal(SIGTERM, klogd_signal);
	signal(SIGHUP, SIG_IGN);

	/* "Open the log. Currently a NOP." */
	klogctl(1, NULL, 0);

	/* Set level of kernel console messaging.. */
	if (console_log_level != -1)
		klogctl(8, NULL, console_log_level);

	syslog(LOG_NOTICE, "klogd started: %s", BB_BANNER);

	while (1) {
		/* Use kernel syscalls */
		memset(log_buffer, '\0', sizeof(log_buffer));
		n = klogctl(2, log_buffer, sizeof(log_buffer));
		if (n < 0) {
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "klogd: Error return from sys_sycall: %d - %m.\n", errno);
			exit(EXIT_FAILURE);
		}

		/* klogctl buffer parsing modelled after code in dmesg.c */
		start = &log_buffer[0];
		lastc = '\0';
		for (i = 0; i < n; i++) {
			if (lastc == '\0' && log_buffer[i] == '<') {
				priority = 0;
				i++;
				while (isdigit(log_buffer[i])) {
					priority = priority * 10 + (log_buffer[i] - '0');
					i++;
				}
				if (log_buffer[i] == '>')
					i++;
				start = &log_buffer[i];
			}
			if (log_buffer[i] == '\n') {
				log_buffer[i] = '\0';	/* zero terminate this message */
				syslog(priority, "%s", start);
				start = &log_buffer[i + 1];
				priority = LOG_INFO;
			}
			lastc = log_buffer[i];
		}
	}
}

#define OPT_LEVEL        1
#define OPT_FOREGROUND   2

int klogd_main(int argc, char **argv)
{
	unsigned long opt;
	char *c_arg;
	int console_log_level = -1;

	/* do normal option parsing */
	opt = bb_getopt_ulflags (argc, argv, "c:n", &c_arg);

	if (opt & OPT_LEVEL) {
		/* Valid levels are between 1 and 8 */
		console_log_level = bb_xgetlarg(c_arg, 10, 1, 8);
	}

	if (!(opt & OPT_FOREGROUND)) {
#if defined(__uClinux__)
		vfork_daemon_rexec(0, 1, argc, argv, "-n");
#else /* __uClinux__ */
		if (daemon(0, 1) < 0)
			bb_perror_msg_and_die("daemon");
#endif /* __uClinux__ */
	}
	doKlogd(console_log_level);

	return EXIT_SUCCESS;
}

/*
Local Variables
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
