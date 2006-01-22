/* vi: set sw=4 ts=4: */
/*
 * Mini halt implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/reboot.h>
#include "busybox.h"
#include "init_shared.h"


extern int halt_main(int argc, char **argv)
{
	char *delay; /* delay in seconds before rebooting */

	if(bb_getopt_ulflags(argc, argv, "d:", &delay)) {
		sleep(atoi(delay));
	}

	return ENABLE_INIT ? kill(1,SIGUSR1) : bb_shutdown_system(RB_HALT_SYSTEM);
}
