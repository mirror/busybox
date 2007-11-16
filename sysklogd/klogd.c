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

#include "libbb.h"
#include <sys/syslog.h>
#include <sys/klog.h>

static void klogd_signal(int sig ATTRIBUTE_UNUSED)
{
	klogctl(7, NULL, 0);
	klogctl(0, NULL, 0);
	syslog(LOG_NOTICE, "klogd: exiting");
	exit(EXIT_SUCCESS);
}

#define log_buffer bb_common_bufsiz1
enum {
	KLOGD_LOGBUF_SIZE = sizeof(log_buffer),
	OPT_LEVEL      = (1 << 0),
	OPT_FOREGROUND = (1 << 1),
};

int klogd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int klogd_main(int argc, char **argv)
{
	int i = i; /* silence gcc */
	char *start;

	/* do normal option parsing */
	getopt32(argv, "c:n", &start);

	if (option_mask32 & OPT_LEVEL) {
		/* Valid levels are between 1 and 8 */
		i = xatoul_range(start, 1, 8);
	}

	if (!(option_mask32 & OPT_FOREGROUND)) {
		bb_daemonize_or_rexec(DAEMON_CHDIR_ROOT, argv);
	}

	openlog("kernel", 0, LOG_KERN);

	/* Set up sig handlers */
	signal(SIGINT, klogd_signal);
	signal(SIGKILL, klogd_signal);
	signal(SIGTERM, klogd_signal);
	signal(SIGHUP, SIG_IGN);

	/* "Open the log. Currently a NOP." */
	klogctl(1, NULL, 0);

	/* Set level of kernel console messaging. */
	if (option_mask32 & OPT_LEVEL)
		klogctl(8, NULL, i);

	syslog(LOG_NOTICE, "klogd started: %s", bb_banner);

	/* Note: this code does not detect incomplete messages
	 * (messages not ending with '\n' or just when kernel
	 * generates too many messages for us to keep up)
	 * and will split them in two separate lines */
	while (1) {
		int n;
		int priority;

		n = klogctl(2, log_buffer, KLOGD_LOGBUF_SIZE - 1);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "klogd: error from klogctl(2): %d - %m",
					errno);
			break;
		}
		log_buffer[n] = '\n';
		i = 0;
		while (i < n) {
			priority = LOG_INFO;
			start = &log_buffer[i];
			if (log_buffer[i] == '<') {
				i++;
				// kernel never ganerates multi-digit prios
				//priority = 0;
				//while (log_buffer[i] >= '0' && log_buffer[i] <= '9') {
				//	priority = priority * 10 + (log_buffer[i] - '0');
				//	i++;
				//}
				if (isdigit(log_buffer[i])) {
					priority = (log_buffer[i] - '0');
					i++;
				}
				if (log_buffer[i] == '>')
					i++;
				start = &log_buffer[i];
			}
			while (log_buffer[i] != '\n')
				i++;
			log_buffer[i] = '\0';
			syslog(priority, "%s", start);
			i++;
		}
	}

	return EXIT_FAILURE;
}
