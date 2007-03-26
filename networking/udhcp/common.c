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

#include <syslog.h>

#include "common.h"


long uptime(void)
{
	struct sysinfo info;
	sysinfo(&info);
	return info.uptime;
}


static const char *saved_pidfile;

static void pidfile_delete(void)
{
	if (saved_pidfile)
		unlink(saved_pidfile);
}

static int pidfile_acquire(const char *pidfile)
{
	int pid_fd;
	if (!pidfile) return -1;

	pid_fd = open(pidfile, O_CREAT|O_WRONLY|O_TRUNC, 0644);
	if (pid_fd < 0) {
		bb_perror_msg("cannot open pidfile %s", pidfile);
	} else {
		/* lockf(pid_fd, F_LOCK, 0); */
		if (!saved_pidfile)
			atexit(pidfile_delete);
		saved_pidfile = pidfile;
	}

	return pid_fd;
}

static void pidfile_write_release(int pid_fd)
{
	if (pid_fd < 0) return;

	fdprintf(pid_fd, "%d\n", getpid());
	/* lockf(pid_fd, F_UNLCK, 0); */
	close(pid_fd);
}


void udhcp_make_pidfile(const char *pidfile)
{
	int pid_fd;

	/* Make sure fd 0,1,2 are open */
	bb_sanitize_stdio();

	/* Equivalent of doing a fflush after every \n */
	setlinebuf(stdout);

	/* Create pidfile */
	pid_fd = pidfile_acquire(pidfile);
	pidfile_write_release(pid_fd);

	bb_info_msg("%s (v%s) started", applet_name, BB_VER);
}
