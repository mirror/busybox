/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2007 Denys Vlasenko
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

pid_t FAST_FUNC xvfork(void)
{
	pid_t pid = vfork();
	if (pid < 0)
		bb_perror_msg_and_die("vfork");
	return pid;
}

#if BB_MMU
pid_t FAST_FUNC xfork(void)
{
	pid_t pid = fork();
	if (pid < 0)
		bb_perror_msg_and_die("vfork" + 1);
	return pid;
}
#endif
