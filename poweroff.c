/* vi: set sw=4 ts=4: */
/*
 * Mini poweroff implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 * Copyright (C) 1999-2002 by Erik Andersen <andersee@debian.org>
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

#include "busybox.h"
#include <signal.h>

extern int poweroff_main(int argc, char **argv)
{
#ifdef BB_FEATURE_LINUXRC
	/* don't assume init's pid == 1 */
	long *pid = find_pid_by_name("init");
	if (!pid || *pid<=0) {
		pid = find_pid_by_name("linuxrc");
		if (!pid || *pid<=0)
			error_msg_and_die("no process killed");
	}
	return(kill(*pid, SIGUSR2));
#else
	return(kill(1, SIGUSR2));
#endif
}
