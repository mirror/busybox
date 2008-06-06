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
#include <syslog.h>
#include <sys/klog.h>

static void klogd_signal(int sig)
{
	/* FYI: cmd 7 is equivalent to setting console_loglevel to 7
	 * via klogctl(8, NULL, 7). */
	klogctl(7, NULL, 0); /* "7 -- Enable printk's to console" */
	klogctl(0, NULL, 0); /* "0 -- Close the log. Currently a NOP" */
	syslog(LOG_NOTICE, "klogd: exiting");
	kill_myself_with_sig(sig);
}

#define log_buffer bb_common_bufsiz1
enum {
	KLOGD_LOGBUF_SIZE = sizeof(log_buffer),
	OPT_LEVEL      = (1 << 0),
	OPT_FOREGROUND = (1 << 1),
};

int klogd_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int klogd_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	int i = 0;
	char *start;
	int opt;

	opt = getopt32(argv, "c:n", &start);
	if (opt & OPT_LEVEL) {
		/* Valid levels are between 1 and 8 */
		i = xatou_range(start, 1, 8);
	}
	if (!(opt & OPT_FOREGROUND)) {
		bb_daemonize_or_rexec(DAEMON_CHDIR_ROOT, argv);
	}

	openlog("kernel", 0, LOG_KERN);

	bb_signals(0
		+ (1 << SIGINT)
		+ (1 << SIGTERM)
		, klogd_signal);
	signal(SIGHUP, SIG_IGN);

	/* "Open the log. Currently a NOP" */
	klogctl(1, NULL, 0);

	/* "printk() prints a message on the console only if it has a loglevel
	 * less than console_loglevel". Here we set console_loglevel = i. */
	if (i)
		klogctl(8, NULL, i);

	syslog(LOG_NOTICE, "klogd started: %s", bb_banner);

	/* Note: this code does not detect incomplete messages
	 * (messages not ending with '\n' or just when kernel
	 * generates too many messages for us to keep up)
	 * and will split them in two separate lines */
	while (1) {
		int n;
		int priority;

		/* "2 -- Read from the log." */
		n = klogctl(2, log_buffer, KLOGD_LOGBUF_SIZE - 1);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			syslog(LOG_ERR, "klogd: error %d in klogctl(2): %m",
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
