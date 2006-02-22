/* vi: set sw=4 ts=4: */
/*
 * Poweroff reboot and halt, oh my.
 *
 * Copyright 2006 by Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <signal.h>
#include <sys/reboot.h>
#include "busybox.h"

#include <unistd.h>

int halt_main(int argc, char *argv[])
{
	static const int magic[] = {RB_HALT_SYSTEM, RB_POWER_OFF, RB_AUTOBOOT};
	static const int signals[] = {SIGUSR1, SIGUSR2, SIGTERM};

	char *delay = "hpr";
	int which, flags, rc = 1;

	/* Figure out which applet we're running */
	for(which=0;delay[which]!=*bb_applet_name;which++);

	/* Parse and handle arguments */
	flags = bb_getopt_ulflags(argc, argv, "d:nf", &delay);
	if (flags&1) sleep(atoi(delay));
	if (!(flags&2)) sync();
	
	/* Perform action. */
	if (ENABLE_INIT && !(flags & 4)) {
		if (ENABLE_FEATURE_INITRD) {
			long *pidlist=find_pid_by_name("linuxrc");
			if (*pidlist>0) rc = kill(*pidlist,signals[which]);
			if (ENABLE_FEATURE_CLEAN_UP) free(pidlist);
		}
		if (rc) rc = kill(1,signals[which]);
	} else rc = reboot(magic[which]);

	if (rc) bb_error_msg("No.");
	return rc;
}
