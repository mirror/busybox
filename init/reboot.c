/* vi: set sw=4 ts=4: */
/*
 * Mini reboot implementation for busybox
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

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include "busybox.h"

#if (__GNU_LIBRARY__ > 5) || defined(__dietlibc__) 
  #include <sys/reboot.h>
  #define init_reboot(magic) reboot(magic)
#else
  #define init_reboot(magic) reboot(0xfee1dead, 672274793, magic)
#endif

#ifndef RB_ENABLE_CAD
static const int RB_ENABLE_CAD = 0x89abcdef;
static const int RB_AUTOBOOT = 0x01234567;
#endif

extern int reboot_main(int argc, char **argv)
{
	int delay = 0; /* delay in seconds before rebooting */
	int rc;

	while ((rc = getopt(argc, argv, "d:")) > 0) {
		switch (rc) {
		case 'd':
			delay = atoi(optarg);
			break;

		default:
			bb_show_usage();
			break;
		}
	}

	if(delay > 0)
		sleep(delay);

#ifdef CONFIG_USER_INIT
		/* Don't kill ourself */
        signal(SIGTERM,SIG_IGN);
        signal(SIGHUP,SIG_IGN);
        setpgrp();

		/* Allow Ctrl-Alt-Del to reboot system. */
		init_reboot(RB_ENABLE_CAD);

		message(CONSOLE|LOG, "\n\rThe system is going down NOW !!\n");
		sync();

		/* Send signals to every process _except_ pid 1 */
		message(CONSOLE|LOG, "\rSending SIGTERM to all processes.\n");
		kill(-1, SIGTERM);
		sleep(1);
		sync();

		message(CONSOLE|LOG, "\rSending SIGKILL to all processes.\n");
		kill(-1, SIGKILL);
		sleep(1);

		sync();
		if (kernelVersion > 0 && kernelVersion <= KERNEL_VERSION(2,2,11)) {
			/* bdflush, kupdate not needed for kernels >2.2.11 */
			bdflush(1, 0);
			sync();
		}

		init_reboot(RB_AUTOBOOT);
		exit(0); /* Shrug */
#else
#ifdef CONFIG_FEATURE_INITRD
	{
		/* don't assume init's pid == 1 */
		long *pid = find_pid_by_name("init");
		if (!pid || *pid<=0)
			pid = find_pid_by_name("linuxrc");
		if (!pid || *pid<=0)
			bb_error_msg_and_die("no process killed");
		fflush(stdout);
		return(kill(*pid, SIGTERM));
	}
#else
	return(kill(1, SIGTERM));
#endif
#endif
}

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
