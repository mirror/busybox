/* common.c
 *
 * Functions to assist in the writing and removing of pidfiles.
 *
 * Russ Dill <Russ.Dill@asu.edu> Soptember 2001
 * Rewrited by Vladimir Oleynik <dzo@simtreas.ru> (C) 2003
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/socket.h>

#include "common.h"


static int daemonized;

#ifdef CONFIG_FEATURE_UDHCP_SYSLOG

void udhcp_logging(int level, const char *fmt, ...)
{
	int e = errno;
	va_list p;
	va_list p2;

	va_start(p, fmt);
	__va_copy(p2, p);
	if(!daemonized) {
		vprintf(fmt, p);
		putchar('\n');
		fflush(stdout);
		errno = e;
	}
	vsyslog(level, fmt, p2);
	va_end(p);
}

void start_log(const char *client_server)
{
	openlog(bb_applet_name, LOG_PID | LOG_CONS, LOG_LOCAL0);
	udhcp_logging(LOG_INFO, "%s (v%s) started", client_server, VERSION);
}

#else

static char *syslog_level_msg[] = {
	[LOG_EMERG]   = "EMERGENCY!",
	[LOG_ALERT]   = "ALERT!",
	[LOG_CRIT]    = "critical!",
	[LOG_WARNING] = "warning",
	[LOG_ERR]     = "error",
	[LOG_INFO]    = "info",
	[LOG_DEBUG]   = "debug"
};

void udhcp_logging(int level, const char *fmt, ...)
{
	int e = errno;
	va_list p;

	va_start(p, fmt);
	if(!daemonized) {
		printf("%s, ", syslog_level_msg[level]);
		errno = e;
		vprintf(fmt, p);
		putchar('\n');
		fflush(stdout);
	}
	va_end(p);
}

void start_log(const char *client_server)
{
	udhcp_logging(LOG_INFO, "%s (v%s) started", client_server, VERSION);
}
#endif

static const char *saved_pidfile;

static void exit_fun(void)
{
	if (saved_pidfile) unlink(saved_pidfile);
}

void background(const char *pidfile)
{
	int pid_fd = -1;

	if (pidfile) {
		pid_fd = open(pidfile, O_CREAT | O_WRONLY, 0644);
		if (pid_fd < 0) {
			LOG(LOG_ERR, "Unable to open pidfile %s: %m", pidfile);
		} else {
			lockf(pid_fd, F_LOCK, 0);
			if(!saved_pidfile)
				atexit(exit_fun);       /* set atexit one only */
			saved_pidfile = pidfile;        /* but may be rewrite */
		}
	}
	while (pid_fd >= 0 && pid_fd < 3) pid_fd = dup(pid_fd); /* don't let daemon close it */
	if (daemon(0, 0) == -1) {
		perror("fork");
		exit(1);
	}
	daemonized++;
	if (pid_fd >= 0) {
		FILE *out;

		if ((out = fdopen(pid_fd, "w")) != NULL) {
			fprintf(out, "%d\n", getpid());
			fclose(out);
		}
		lockf(pid_fd, F_UNLCK, 0);
		close(pid_fd);
	}
}

/* Signal handler */
int udhcp_signal_pipe[2];
static void signal_handler(int sig)
{
	if (send(udhcp_signal_pipe[1], &sig, sizeof(sig), MSG_DONTWAIT) < 0) {
		LOG(LOG_ERR, "Could not send signal: %m");
	}
}

void udhcp_set_signal_pipe(int sig_add)
{
	socketpair(AF_UNIX, SOCK_STREAM, 0, udhcp_signal_pipe);
	signal(SIGUSR1, signal_handler);
	signal(SIGTERM, signal_handler);
	if(sig_add)
		signal(sig_add, signal_handler);
}
