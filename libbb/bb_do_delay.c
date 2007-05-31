/* vi: set sw=4 ts=4: */
/*
 * Busybox utility routines.
 *
 * Copyright (C) 2005 by Tito Ragusa <tito-wolit@tiscali.it>
 *
 * Licensed under the GPL v2, see the file LICENSE in this tarball.
 */

#include "libbb.h"

void bb_do_delay(int seconds)
{
	time_t start, now;

	time(&start);
	now = start;
	while (difftime(now, start) < seconds) {
		sleep(seconds);
		time(&now);
	}
}
