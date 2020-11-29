/* vi: set sw=4 ts=4: */
/*
 * Busybox utility routines.
 *
 * Copyright (C) 2005 by Tito Ragusa <tito-wolit@tiscali.it>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
#include "libbb.h"

/* void FAST_FUNC bb_do_delay(int seconds) { ... } - no users yet */

#ifndef LOGIN_FAIL_DELAY
#define LOGIN_FAIL_DELAY 3
#endif
void FAST_FUNC pause_after_failed_login(void)
{
#if 0 /* over-engineered madness */
	time_t end, diff;

	end = time(NULL) + LOGIN_FAIL_DELAY;
	diff = LOGIN_FAIL_DELAY;
	do {
		sleep(diff);
		diff = end - time(NULL);
	} while (diff > 0);
#else
	sleep(LOGIN_FAIL_DELAY);
#endif
}
