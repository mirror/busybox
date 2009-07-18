/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 2007 Denys Vlasenko
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */
#include "libbb.h"

void FAST_FUNC parse_datestr(const char *date_str, struct tm *tm_time)
{
	char end = '\0';
	const char *last_colon = strrchr(date_str, ':');

	if (last_colon != NULL) {
		/* Parse input and assign appropriately to tm_time */

		if (sscanf(date_str, "%u:%u%c",
					&tm_time->tm_hour,
					&tm_time->tm_min,
					&end) >= 2) {
			/* no adjustments needed */
		} else if (sscanf(date_str, "%u.%u-%u:%u%c",
					&tm_time->tm_mon, &tm_time->tm_mday,
					&tm_time->tm_hour, &tm_time->tm_min,
					&end) >= 4) {
			/* Adjust dates from 1-12 to 0-11 */
			tm_time->tm_mon -= 1;
		} else if (sscanf(date_str, "%u.%u.%u-%u:%u%c", &tm_time->tm_year,
					&tm_time->tm_mon, &tm_time->tm_mday,
					&tm_time->tm_hour, &tm_time->tm_min,
					&end) >= 5) {
			tm_time->tm_year -= 1900; /* Adjust years */
			tm_time->tm_mon -= 1; /* Adjust dates from 1-12 to 0-11 */
		} else if (sscanf(date_str, "%u-%u-%u %u:%u%c", &tm_time->tm_year,
					&tm_time->tm_mon, &tm_time->tm_mday,
					&tm_time->tm_hour, &tm_time->tm_min,
					&end) >= 5) {
			tm_time->tm_year -= 1900; /* Adjust years */
			tm_time->tm_mon -= 1; /* Adjust dates from 1-12 to 0-11 */
//TODO: coreutils 6.9 also accepts "YYYY-MM-DD HH" (no minutes)
		} else {
			bb_error_msg_and_die(bb_msg_invalid_date, date_str);
		}
		if (end == ':') {
			if (sscanf(last_colon + 1, "%u%c", &tm_time->tm_sec, &end) == 1)
				end = '\0';
			/* else end != NUL and we error out */
		}
	} else {
		if (sscanf(date_str, "%2u%2u%2u%2u%u%c", &tm_time->tm_mon,
				&tm_time->tm_mday, &tm_time->tm_hour, &tm_time->tm_min,
				&tm_time->tm_year, &end) < 4)
			bb_error_msg_and_die(bb_msg_invalid_date, date_str);
		/* correct for century  - minor Y2K problem here? */
		if (tm_time->tm_year >= 1900) {
			tm_time->tm_year -= 1900;
		}
		/* adjust date */
		tm_time->tm_mon -= 1;
		if (end == '.') {
			if (sscanf(strchr(date_str, '.') + 1, "%u%c",
					&tm_time->tm_sec, &end) == 1)
				end = '\0';
			/* else end != NUL and we error out */
		}
	}
	if (end != '\0') {
		bb_error_msg_and_die(bb_msg_invalid_date, date_str);
	}
}

time_t FAST_FUNC validate_tm_time(const char *date_str, struct tm *tm_time)
{
	time_t t = mktime(tm_time);
	if (t == (time_t) -1L) {
		bb_error_msg_and_die(bb_msg_invalid_date, date_str);
	}
	return t;
}

#if ENABLE_MONOTONIC_SYSCALL

#include <sys/syscall.h>
/* Old glibc (< 2.3.4) does not provide this constant. We use syscall
 * directly so this definition is safe. */
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

/* libc has incredibly messy way of doing this,
 * typically requiring -lrt. We just skip all this mess */
static void get_mono(struct timespec *ts)
{
	if (syscall(__NR_clock_gettime, CLOCK_MONOTONIC, ts))
		bb_error_msg_and_die("clock_gettime(MONOTONIC) failed");
}
unsigned long long FAST_FUNC monotonic_ns(void)
{
	struct timespec ts;
	get_mono(&ts);
	return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}
unsigned long long FAST_FUNC monotonic_us(void)
{
	struct timespec ts;
	get_mono(&ts);
	return ts.tv_sec * 1000000ULL + ts.tv_nsec/1000;
}
unsigned FAST_FUNC monotonic_sec(void)
{
	struct timespec ts;
	get_mono(&ts);
	return ts.tv_sec;
}

#else

unsigned long long FAST_FUNC monotonic_ns(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000000ULL + tv.tv_usec * 1000;
}
unsigned long long FAST_FUNC monotonic_us(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000ULL + tv.tv_usec;
}
unsigned FAST_FUNC monotonic_sec(void)
{
	return time(NULL);
}

#endif
