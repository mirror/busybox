/* vi: set sw=4 ts=4: */
/*
 * Mini update implementation for busybox
 *
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
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

#include "internal.h"
#include <linux/unistd.h>

#if defined(__GLIBC__)
#include <sys/kdaemon.h>
#else
_syscall2(int, bdflush, int, func, int, data);
#endif							/* __GLIBC__ */

extern int update_main(int argc, char **argv)
{
	/*
	 * Update is actually two daemons, bdflush and update.
	 */
	int pid;

	pid = fork();
	if (pid < 0)
		return pid;
	else if (pid == 0) {
		/*
		 * This is no longer necessary since 1.3.5x, but it will harmlessly
		 * exit if that is the case.
		 */
		strcpy(argv[0], "bdflush (update)");
		argv[1] = 0;
		argv[2] = 0;
		bdflush(1, 0);
		_exit(0);
	}
	pid = fork();
	if (pid < 0)
		return pid;
	else if (pid == 0) {
		argv[0] = "update";
		for (;;) {
			sync();
			sleep(30);
		}
	}

	return 0;
}
