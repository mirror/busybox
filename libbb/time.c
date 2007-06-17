/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2007 Denis Vlasenko
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

#include "libbb.h"

#if ENABLE_MONOTONIC_SYSCALL
#include <sys/syscall.h>

/* libc has incredibly messy way of doing this,
 * typically requiring -lrt. We just skip all this mess */
unsigned long long monotonic_us(void)
{
	struct timespec ts;
	if (syscall(__NR_clock_gettime, CLOCK_MONOTONIC, &ts))
		bb_error_msg_and_die("clock_gettime(MONOTONIC) failed");
	return ts.tv_sec * 1000000ULL + ts.tv_nsec/1000;
}
#else
unsigned long long monotonic_us(void)
{
	struct timeval tv;
	if (gettimeofday(&tv, NULL))
		bb_error_msg_and_die("gettimeofday failed");
	return tv.tv_sec * 1000000ULL + tv_usec;
}
#endif
