/* vi: set sw=4 ts=4: */
/*
 * Mini klogd implementation for busybox
 *
 * Copyright (C) 2001 by Gennady Feldman <gfeldman@cachier.com>.
 * Changes: Made this a standalone busybox module which uses standalone
 * 					syslog() client interface.
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
 *
 * Copyright (C) 2000 by Karl M. Hegbloom <karlheg@debian.org>
 *
 * "circular buffer" Copyright (C) 2000 by Gennady Feldman <gfeldman@mail.com>
 *
 * Maintainer: Gennady Feldman <gena01@cachier.com> as of Mar 12, 2001
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h> /* for our signal() handlers */
#include <string.h> /* strncpy() */
#include <errno.h>  /* errno and friends */
#include <unistd.h>
#include <ctype.h>
#include <sys/syslog.h>

#if __GNU_LIBRARY__ < 5
# ifdef __alpha__
#   define klogctl syslog
# endif
#else
# include <sys/klog.h>
#endif

#include "busybox.h"

static void klogd_signal(int sig)
{
	klogctl(7, NULL, 0);
	klogctl(0, 0, 0);
	//logMessage(0, "Kernel log daemon exiting.");
	syslog_msg(LOG_DAEMON, 0, "Kernel log daemon exiting.");
	exit(TRUE);
}

static void doKlogd (void) __attribute__ ((noreturn));
static void doKlogd (void)
{
	int priority = LOG_INFO;
	char log_buffer[4096];
	int i, n, lastc;
	char *start;

	/* Set up sig handlers */
	signal(SIGINT, klogd_signal);
	signal(SIGKILL, klogd_signal);
	signal(SIGTERM, klogd_signal);
	signal(SIGHUP, SIG_IGN);

	/* "Open the log. Currently a NOP." */
	klogctl(1, NULL, 0);

	syslog_msg(LOG_DAEMON, 0, "klogd started: " BB_BANNER);

	while (1) {
		/* Use kernel syscalls */
		memset(log_buffer, '\0', sizeof(log_buffer));
		n = klogctl(2, log_buffer, sizeof(log_buffer));
		if (n < 0) {
			char message[80];

			if (errno == EINTR)
				continue;
			snprintf(message, 79, "klogd: Error return from sys_sycall: %d - %s.\n", 
												errno, strerror(errno));
			syslog_msg(LOG_DAEMON, LOG_SYSLOG | LOG_ERR, message);
			exit(1);
		}

		/* klogctl buffer parsing modelled after code in dmesg.c */
		start=&log_buffer[0];
		lastc='\0';
		for (i=0; i<n; i++) {
			if (lastc == '\0' && log_buffer[i] == '<') {
				priority = 0;
				i++;
				while (isdigit(log_buffer[i])) {
					priority = priority*10+(log_buffer[i]-'0');
					i++;
				}
				if (log_buffer[i] == '>') i++;
				start = &log_buffer[i];
			}
			if (log_buffer[i] == '\n') {
				log_buffer[i] = '\0';  /* zero terminate this message */
				syslog_msg(LOG_DAEMON, LOG_KERN | priority, start);
				start = &log_buffer[i+1];
				priority = LOG_INFO;
			}
			lastc = log_buffer[i];
		}
	}
}

extern int klogd_main(int argc, char **argv)
{
	/* no options, no getopt */
	int opt;
	int doFork = TRUE;

	/* do normal option parsing */
	while ((opt = getopt(argc, argv, "n")) > 0) {
		switch (opt) {
			case 'n':
				doFork = FALSE;
				break;
			default:
				show_usage();
		}
	}

	if (doFork) {
#if !defined(__UCLIBC__) || defined(__UCLIBC_HAS_MMU__)
		if (daemon(0, 1) < 0)
			perror_msg_and_die("daemon");
#else
			error_msg_and_die("daemon not supported");
#endif
	}
	doKlogd();
	
	return EXIT_SUCCESS;
}

/*
Local Variables
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
