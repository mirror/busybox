/* vi: set sw=4 ts=4: */
/* common.c
 *
 * Functions for debugging and logging as well as some other
 * simple helper functions.
 *
 * Russ Dill <Russ.Dill@asu.edu> 2001-2003
 * Rewritten by Vladimir Oleynik <dzo@simtreas.ru> (C) 2003
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <paths.h>
#include <sys/socket.h>
#include <stdarg.h>
#include <syslog.h>

#include "common.h"
#include "pidfile.h"


static int daemonized;

long uptime(void)
{
	struct sysinfo info;
	sysinfo(&info);
	return info.uptime;
}

/*
 * This function makes sure our first socket calls
 * aren't going to fd 1 (printf badness...) and are
 * not later closed by daemon()
 */
static inline void sanitize_fds(void)
{
	int fd = open(bb_dev_null, O_RDWR, 0);
	if (fd < 0)
		return;
	while (fd < 3)
		fd = dup(fd);
	close(fd);
}


void udhcp_background(const char *pidfile)
{
#ifdef __uClinux__
	bb_error_msg("Cannot background in uclinux (yet)");
#else /* __uClinux__ */
	int pid_fd;

	/* hold lock during fork. */
	pid_fd = pidfile_acquire(pidfile);
	setsid();
	xdaemon(0, 0);
	daemonized++;
	logmode &= ~LOGMODE_STDIO;
	pidfile_write_release(pid_fd);
#endif /* __uClinux__ */
}

void udhcp_start_log_and_pid(const char *pidfile)
{
	int pid_fd;

	/* Make sure our syslog fd isn't overwritten */
	sanitize_fds();

	/* do some other misc startup stuff while we are here to save bytes */
	pid_fd = pidfile_acquire(pidfile);
	pidfile_write_release(pid_fd);

	/* equivelent of doing a fflush after every \n */
	setlinebuf(stdout);

	if (ENABLE_FEATURE_UDHCP_SYSLOG) {
		openlog(applet_name, LOG_PID, LOG_LOCAL0);
		logmode |= LOGMODE_SYSLOG;
	}

	bb_info_msg("%s (v%s) started", applet_name, BB_VER);
}
