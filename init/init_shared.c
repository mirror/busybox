/* vi: set sw=4 ts=4: */
/*
 * Stuff shared between init, reboot, halt, and poweroff
 *
 * Copyright (C) 1999-2003 by Erik Andersen <andersen@codepoet.org>
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

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/reboot.h>
#include <sys/syslog.h>
#include "busybox.h"
#include "init_shared.h"

extern int kill_init(int sig)
{
#ifdef CONFIG_FEATURE_INITRD
	/* don't assume init's pid == 1 */
	long *pid = find_pid_by_name("init");
	if (!pid || *pid<=0) {
		pid = find_pid_by_name("linuxrc");
		if (!pid || *pid<=0)
			bb_error_msg_and_die("no process killed");
	}
	return(kill(*pid, sig));
#else
	return(kill(1, sig));
#endif
}

#ifndef CONFIG_INIT
const char * const bb_shutdown_format = "\r%s\n";
extern int bb_shutdown_system(unsigned long magic)
{
	int pri = LOG_KERN|LOG_NOTICE|LOG_FACMASK;
	const char *message;

	/* Don't kill ourself */
	signal(SIGTERM,SIG_IGN);
	signal(SIGHUP,SIG_IGN);
	setpgrp();

	/* Allow Ctrl-Alt-Del to reboot system. */
#ifndef RB_ENABLE_CAD
#define RB_ENABLE_CAD	0x89abcdef
#endif
	reboot(RB_ENABLE_CAD);

	openlog(bb_applet_name, 0, pri);

	message = "\nThe system is going down NOW !!";
	syslog(pri, "%s", message);
	printf(bb_shutdown_format, message);

	sync();

	/* Send signals to every process _except_ pid 1 */
	message = "Sending SIGTERM to all processes.";
	syslog(pri, "%s", message);
	printf(bb_shutdown_format, message);

	kill(-1, SIGTERM);
	sleep(1);
	sync();

	message = "Sending SIGKILL to all processes.";
	syslog(pri, "%s", message);
	printf(bb_shutdown_format, message);

	kill(-1, SIGKILL);
	sleep(1);

	sync();

	reboot(magic);
	return 0; /* Shrug */
}
#endif
