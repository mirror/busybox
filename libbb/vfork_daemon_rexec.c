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
void vfork_daemon_rexec(int nochdir, int noclose,
		int argc, char **argv, char *foreground_opt)
{
	int fd;
	char **vfork_args;
	int a = 0;

	setsid();

	if (!nochdir)
		xchdir("/");

	if (!noclose && (fd = open(bb_dev_null, O_RDWR, 0)) != -1) {
		dup2(fd, STDIN_FILENO);
		dup2(fd, STDOUT_FILENO);
		dup2(fd, STDERR_FILENO);
		while (fd > 2)
			close(fd--);
	}

	vfork_args = xzalloc(sizeof(char *) * (argc + 3));
	vfork_args[a++] = CONFIG_BUSYBOX_EXEC_PATH;
	while (*argv) {
		vfork_args[a++] = *argv;
		argv++;
	}
	vfork_args[a] = foreground_opt;
	switch (vfork()) {
	case 0: /* child */
		/* Make certain we are not a session leader, or else we
		 * might reacquire a controlling terminal */
		if (vfork())
			_exit(0);
		execv(vfork_args[0], vfork_args);
		bb_perror_msg_and_die("execv %s", vfork_args[0]);
	case -1: /* error */
		bb_perror_msg_and_die("vfork");
	default: /* parent */
		exit(0);
	}
}
#endif /* BB_NOMMU */
