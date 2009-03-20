/* vi: set sw=4 ts=4: */
/*
 * setsid.c -- execute a command in a new session
 * Rick Sladkey <jrs@world.std.com>
 * In the public domain.
 *
 * 1999-02-22 Arkadiusz Mickiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 *
 * 2001-01-18 John Fremlin <vii@penguinpowered.com>
 * - fork in case we are process group leader
 *
 * 2004-11-12 Paul Fox
 * - busyboxed
 */

#include "libbb.h"

int setsid_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int setsid_main(int argc UNUSED_PARAM, char **argv)
{
	if (!argv[1])
		bb_show_usage();

	/* setsid() is allowed only when we are not a process group leader.
	 * Otherwise our PID serves as PGID of some existing process group
	 * and cannot be used as PGID of a new process group. */
	if (getpgrp() == getpid())
		if (fork_or_rexec(argv))
			exit(EXIT_SUCCESS); /* parent */

	setsid();  /* no error possible */

	BB_EXECVP(argv[1], argv + 1);
	bb_simple_perror_msg_and_die(argv[1]);
}
