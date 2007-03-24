/* vi: set sw=4 ts=4: */
/*
 * Rexec program for system have fork() as vfork() with foreground option
 *
 * Copyright (C) Vladimir N. Oleynik <dzo@simtreas.ru>
 * Copyright (C) 2003 Russ Dill <Russ.Dill@asu.edu>
 *
 * daemon() portion taken from uClibc:
 *
 * Copyright (c) 1991, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Modified for uClibc by Erik Andersen <andersee@debian.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <paths.h>
#include "libbb.h"

#ifdef BB_NOMMU
void vfork_daemon_rexec(int nochdir, int noclose, char **argv)
{
	int fd;

	setsid();

	if (!nochdir)
		xchdir("/");

	if (!noclose) {
		/* if "/dev/null" doesn't exist, bail out! */
		fd = xopen(bb_dev_null, O_RDWR);
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		while (fd > 2)
			close(fd--);
	}

	switch (vfork()) {
	case 0: /* child */
		/* Make certain we are not a session leader, or else we
		 * might reacquire a controlling terminal */
		if (vfork())
			_exit(0);
		/* High-order bit of first char in argv[0] is a hidden
		 * "we have (alrealy) re-execed, don't do it again" flag */
		argv[0][0] |= 0x80;
		execv(CONFIG_BUSYBOX_EXEC_PATH, argv);
		bb_perror_msg_and_die("exec %s", CONFIG_BUSYBOX_EXEC_PATH);
	case -1: /* error */
		bb_perror_msg_and_die("vfork");
	default: /* parent */
		exit(0);
	}
}
#endif /* BB_NOMMU */
