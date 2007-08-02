/* vi: set sw=4 ts=4: */
/*
 * pid file routines
 *
 * Copyright (C) 2007 by Stephane Billiart <stephane.billiart@gmail.com>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* Override ENABLE_FEATURE_PIDFILE */
#define WANT_PIDFILE 1
#include "libbb.h"

int write_pidfile(const char *path)
{
	int pid_fd;
	char *end;
	char buf[sizeof(int)*3 + 2];

	if (!path)
		return 1;
	/* we will overwrite stale pidfile */
	pid_fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (pid_fd < 0)
		return 0;
	/* few bytes larger, but doesn't use stdio */
	end = utoa_to_buf(getpid(), buf, sizeof(buf));
	*end = '\n';
	full_write(pid_fd, buf, end - buf + 1);
	close(pid_fd);
	return 1;
}
