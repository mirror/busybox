/* vi: set sw=4 ts=4: */
/*
 * Mini hwclock implementation for busybox
 *
 * Copyright (C) 2002 Robert Griebl <griebl@gmx.de>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
*/


#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <getopt.h>
#include "busybox.h"

/* Copied from linux/rtc.h to eliminate the kernel dependency */
struct linux_rtc_time {
	int tm_sec;
	int tm_min;
	int tm_hour;
	int tm_mday;
	int tm_mon;
	int tm_year;
	int tm_wday;
	int tm_yday;
	int tm_isdst;
};

#define RTC_SET_TIME   _IOW('p', 0x0a, struct linux_rtc_time) /* Set RTC time    */
#define RTC_RD_TIME    _IOR('p', 0x09, struct linux_rtc_time) /* Read RTC time   */

#if ENABLE_FEATURE_HWCLOCK_LONG_OPTIONS
# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
#endif

static int xopen_rtc(int flags)
{
	int rtc;
	rtc = open("/dev/rtc", flags);
	if (rtc < 0) {
		rtc = open("/dev/misc/rtc", flags);
		if (rtc < 0)
			bb_perror_msg_and_die("cannot access RTC");
	}
	return rtc;
}

static time_t read_rtc(int utc)
{
	struct tm tm;
	char *oldtz = 0;
	time_t t = 0;
	int rtc = xopen_rtc(O_RDONLY);

	memset(&tm, 0, sizeof(struct tm));
	if (ioctl(rtc, RTC_RD_TIME, &tm) < 0 )
		bb_perror_msg_and_die("cannot read time from RTC");
	tm.tm_isdst = -1; /* not known */

	close(rtc);

	if (utc) {
		oldtz = getenv("TZ");
		setenv("TZ", "UTC 0", 1);
		tzset();
	}

	t = mktime(&tm);

	if (utc) {
		if (oldtz)
			setenv("TZ", oldtz, 1);
		else
			unsetenv("TZ");
		tzset();
	}
	return t;
}

static void write_rtc(time_t t, int utc)
{
	struct tm tm;
	int rtc = xopen_rtc(O_WRONLY);

	tm = *(utc ? gmtime(&t) : localtime(&t));
	tm.tm_isdst = 0;

	if (ioctl(rtc, RTC_SET_TIME, &tm) < 0)
		bb_perror_msg_and_die("cannot set the RTC time");

	close(rtc);
}

static int show_clock(int utc)
{
	struct tm *ptm;
	time_t t;
	RESERVE_CONFIG_BUFFER(buffer, 64);

	t = read_rtc(utc);
	ptm = localtime(&t);  /* Sets 'tzname[]' */

	safe_strncpy(buffer, ctime(&t), 64);
	if (buffer[0])
		buffer[strlen(buffer) - 1] = 0;

	//printf("%s  %.6f seconds %s\n", buffer, 0.0, utc ? "" : (ptm->tm_isdst ? tzname[1] : tzname[0]));
	printf( "%s  %.6f seconds\n", buffer, 0.0);
	RELEASE_CONFIG_BUFFER(buffer);

	return 0;
}

static int to_sys_clock(int utc)
{
	struct timeval tv = { 0, 0 };
	const struct timezone tz = { timezone/60 - 60*daylight, 0 };

	tv.tv_sec = read_rtc(utc);

	if (settimeofday(&tv, &tz))
		bb_perror_msg_and_die("settimeofday() failed");

	return 0;
}

static int from_sys_clock(int utc)
{
	struct timeval tv = { 0, 0 };
	struct timezone tz = { 0, 0 };

	if (gettimeofday(&tv, &tz))
		bb_perror_msg_and_die("gettimeofday() failed");

	write_rtc(tv.tv_sec, utc);
	return 0;
}

#ifdef CONFIG_FEATURE_HWCLOCK_ADJTIME_FHS
# define ADJTIME_PATH "/var/lib/hwclock/adjtime"
#else
# define ADJTIME_PATH "/etc/adjtime"
#endif
static int check_utc(void)
{
	int utc = 0;
	FILE *f = fopen(ADJTIME_PATH, "r");

	if (f) {
		RESERVE_CONFIG_BUFFER(buffer, 128);

		while (fgets(buffer, sizeof(buffer), f)) {
			int len = strlen(buffer);

			while (len && isspace(buffer[len - 1]))
				len--;

			buffer[len] = 0;

			if (strncmp(buffer, "UTC", 3) == 0 ) {
				utc = 1;
				break;
			}
		}
		fclose(f);
		RELEASE_CONFIG_BUFFER(buffer);
	}
	return utc;
}

#define HWCLOCK_OPT_LOCALTIME   0x01
#define HWCLOCK_OPT_UTC         0x02
#define HWCLOCK_OPT_SHOW        0x04
#define HWCLOCK_OPT_HCTOSYS     0x08
#define HWCLOCK_OPT_SYSTOHC     0x10

int hwclock_main(int argc, char **argv )
{
	unsigned opt;
	int utc;

#if ENABLE_FEATURE_HWCLOCK_LONG_OPTIONS
	static const struct option hwclock_long_options[] = {
		{ "localtime", 0, 0, 'l' },
		{ "utc",       0, 0, 'u' },
		{ "show",      0, 0, 'r' },
		{ "hctosys",   0, 0, 's' },
		{ "systohc",   0, 0, 'w' },
		{ 0,           0, 0, 0 }
	};
	applet_long_options = hwclock_long_options;
#endif
	opt_complementary = "?:r--ws:w--rs:s--wr:l--u:u--l";
	opt = getopt32(argc, argv, "lursw");

	/* If -u or -l wasn't given check if we are using utc */
	if (opt & (HWCLOCK_OPT_UTC | HWCLOCK_OPT_LOCALTIME))
		utc = opt & HWCLOCK_OPT_UTC;
	else
		utc = check_utc();

	if (opt & HWCLOCK_OPT_HCTOSYS) {
		return to_sys_clock(utc);
	}
	else if (opt & HWCLOCK_OPT_SYSTOHC) {
		return from_sys_clock(utc);
	} else {
		/* default HWCLOCK_OPT_SHOW */
		return show_clock(utc);
	}
}
