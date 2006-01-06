/* vi: set sw=4 ts=4: */
/*
 * Busybox utility routines.
 *
 * Copyright (C) 2005 by Tito Ragusa <tito-wolit@tiscali.it>
 *
 * Licensed under the GPL v2, see the file LICENSE in this tarball.
 */

#include <time.h>
#include <unistd.h>

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

/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
