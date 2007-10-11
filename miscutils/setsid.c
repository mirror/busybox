/* vi: set sw=4 ts=4: */
/*
 * setsid.c -- execute a command in a new session
 * Rick Sladkey <jrs@world.std.com>
 * In the public domain.
 *
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
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
int setsid_main(int argc, char **argv)
{
	if (argc < 2)
		bb_show_usage();

	/* Comment why is this necessary? */
	if (getpgrp() == getpid())
		forkexit_or_rexec(argv);

	setsid();  /* no error possible */

	BB_EXECVP(argv[1], argv + 1);
	bb_simple_perror_msg_and_die(argv[1]);
}
