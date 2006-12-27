/* vi: set sw=4 ts=4: */
/*
 * Stuff shared between init, reboot, halt, and poweroff
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include <sys/reboot.h>
#include <sys/syslog.h>
#include "init_shared.h"

const char * const init_sending_format = "Sending SIG%s to all processes.";
#ifndef CONFIG_INIT
const char * const bb_shutdown_format = "\r%s\n";
int bb_shutdown_system(unsigned long magic)
{
	int pri = LOG_KERN|LOG_NOTICE|LOG_FACMASK;
	const char *message;

	/* Don't kill ourself */
	signal(SIGTERM,SIG_IGN);
	signal(SIGHUP,SIG_IGN);
	bb_setpgrp;

	/* Allow Ctrl-Alt-Del to reboot system. */
#ifndef RB_ENABLE_CAD
#define RB_ENABLE_CAD	0x89abcdef
#endif
	reboot(RB_ENABLE_CAD);

	openlog(applet_name, 0, pri);

	message = "\nThe system is going down NOW !!";
	syslog(pri, "%s", message);
	printf(bb_shutdown_format, message);

	sync();

	/* Send signals to every process _except_ pid 1 */
	message = "TERM";
	syslog(pri, init_sending_format, message);
	printf(bb_shutdown_format, message);

	kill(-1, SIGTERM);
	sleep(1);
	sync();

	message = "KILL";
	syslog(pri, init_sending_format, message);
	printf(bb_shutdown_format, message);

	kill(-1, SIGKILL);
	sleep(1);

	sync();

	reboot(magic);
	return 0; /* Shrug */
}
#endif
