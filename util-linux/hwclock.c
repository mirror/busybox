/* vi: set sw=4 ts=4: */
/*
 * Mini hwclock implementation for busybox
 *
 * Copyright (C) 2002 Robert Griebl <griebl@gmx.de>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
*/

#include <sys/utsname.h>
#include "libbb.h"
#include "rtc_.h"

#if ENABLE_FEATURE_HWCLOCK_LONG_OPTIONS
# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
#endif

static const char *rtcname;

static time_t read_rtc(int utc)
{
	time_t ret;
	int fd;

	fd = rtc_xopen(&rtcname, O_RDONLY);
	ret = rtc_read_time(fd, utc);
	close(fd);

	return ret;
}

static void write_rtc(time_t t, int utc)
{
	struct tm tm;
	int rtc = rtc_xopen(&rtcname, O_WRONLY);

	tm = *(utc ? gmtime(&t) : localtime(&t));
	tm.tm_isdst = 0;

	xioctl(rtc, RTC_SET_TIME, &tm);

	close(rtc);
}

static void show_clock(int utc)
{
	//struct tm *ptm;
	time_t t;
	char *cp;

	t = read_rtc(utc);
	//ptm = localtime(&t);  /* Sets 'tzname[]' */

	cp = ctime(&t);
	if (cp[0])
		cp[strlen(cp) - 1] = '\0';

	//printf("%s  %.6f seconds %s\n", cp, 0.0, utc ? "" : (ptm->tm_isdst ? tzname[1] : tzname[0]));
	printf("%s  0.000000 seconds\n", cp);
}

static void to_sys_clock(int utc)
{
	struct timeval tv;
	const struct timezone tz = { timezone/60 - 60*daylight, 0 };

	tv.tv_sec = read_rtc(utc);
	tv.tv_usec = 0;
	if (settimeofday(&tv, &tz))
		bb_perror_msg_and_die("settimeofday() failed");
}

static void from_sys_clock(int utc)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	//if (gettimeofday(&tv, NULL))
	//	bb_perror_msg_and_die("gettimeofday() failed");
	write_rtc(tv.tv_sec, utc);
}

#define HWCLOCK_OPT_LOCALTIME   0x01
#define HWCLOCK_OPT_UTC         0x02
#define HWCLOCK_OPT_SHOW        0x04
#define HWCLOCK_OPT_HCTOSYS     0x08
#define HWCLOCK_OPT_SYSTOHC     0x10
#define HWCLOCK_OPT_RTCFILE     0x20

int hwclock_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int hwclock_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	unsigned opt;
	int utc;

#if ENABLE_FEATURE_HWCLOCK_LONG_OPTIONS
	static const char hwclock_longopts[] ALIGN1 =
		"localtime\0" No_argument "l"
		"utc\0"       No_argument "u"
		"show\0"      No_argument "r"
		"hctosys\0"   No_argument "s"
		"systohc\0"   No_argument "w"
		"file\0"      Required_argument "f"
		;
	applet_long_options = hwclock_longopts;
#endif
	opt_complementary = "r--ws:w--rs:s--wr:l--u:u--l";
	opt = getopt32(argv, "lurswf:", &rtcname);

	/* If -u or -l wasn't given check if we are using utc */
	if (opt & (HWCLOCK_OPT_UTC | HWCLOCK_OPT_LOCALTIME))
		utc = (opt & HWCLOCK_OPT_UTC);
	else
		utc = rtc_adjtime_is_utc();

	if (opt & HWCLOCK_OPT_HCTOSYS)
		to_sys_clock(utc);
	else if (opt & HWCLOCK_OPT_SYSTOHC)
		from_sys_clock(utc);
	else
		/* default HWCLOCK_OPT_SHOW */
		show_clock(utc);

	return 0;
}
