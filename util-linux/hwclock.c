/* vi: set sw=4 ts=4: */
/*
 * Mini hwclock implementation for busybox
 *
 * Copyright (C) 2002 Robert Griebl <griebl@gmx.de>
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


#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <ctype.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include "busybox.h"

/* Copied from linux/rtc.h to eliminate the kernel dependancy */
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

#ifdef CONFIG_FEATURE_HWCLOCK_LONGOPTIONS
# ifndef _GNU_SOURCE
#  define _GNU_SOURCE
# endif
#endif

static time_t read_rtc(int utc)
{
	int rtc;
	struct tm tm;
	char *oldtz = 0;
	time_t t = 0;

	if (( rtc = open ( "/dev/rtc", O_RDONLY )) < 0 ) {
		if (( rtc = open ( "/dev/misc/rtc", O_RDONLY )) < 0 )
			bb_perror_msg_and_die ( "Could not access RTC" );
	}
 	memset ( &tm, 0, sizeof( struct tm ));
	if ( ioctl ( rtc, RTC_RD_TIME, &tm ) < 0 )
		bb_perror_msg_and_die ( "Could not read time from RTC" );
	tm. tm_isdst = -1; // not known

	close ( rtc );

	if ( utc ) {
		oldtz = getenv ( "TZ" );
		setenv ( "TZ", "UTC 0", 1 );
		tzset ( );
	}

	t = mktime ( &tm );

	if ( utc ) {
		if ( oldtz )
			setenv ( "TZ", oldtz, 1 );
		else
			unsetenv ( "TZ" );
		tzset ( );
	}
	return t;
}

static void write_rtc(time_t t, int utc)
{
	int rtc;
	struct tm tm;

	if (( rtc = open ( "/dev/rtc", O_WRONLY )) < 0 ) {
		if (( rtc = open ( "/dev/misc/rtc", O_WRONLY )) < 0 )
			bb_perror_msg_and_die ( "Could not access RTC" );
	}

	tm = *( utc ? gmtime ( &t ) : localtime ( &t ));
	tm. tm_isdst = 0;

	if ( ioctl ( rtc, RTC_SET_TIME, &tm ) < 0 )
		bb_perror_msg_and_die ( "Could not set the RTC time" );

	close ( rtc );
}

static int show_clock(int utc)
{
	struct tm *ptm;
	time_t t;
	char buffer [64];

	t = read_rtc ( utc );
	ptm = localtime ( &t );  /* Sets 'tzname[]' */

	safe_strncpy ( buffer, ctime ( &t ), sizeof( buffer ));
	if ( buffer [0] )
		buffer [bb_strlen ( buffer ) - 1] = 0;

	//printf ( "%s  %.6f seconds %s\n", buffer, 0.0, utc ? "" : ( ptm-> tm_isdst ? tzname [1] : tzname [0] ));
	printf ( "%s  %.6f seconds\n", buffer, 0.0 );

	return 0;
}

static int to_sys_clock(int utc)
{
	struct timeval tv = { 0, 0 };
	const struct timezone tz = { timezone/60 - 60*daylight, 0 };

	tv. tv_sec = read_rtc ( utc );

	if ( settimeofday ( &tv, &tz ))
		bb_perror_msg_and_die ( "settimeofday() failed" );

	return 0;
}

static int from_sys_clock(int utc)
{
	struct timeval tv = { 0, 0 };
	struct timezone tz = { 0, 0 };

	if ( gettimeofday ( &tv, &tz ))
		bb_perror_msg_and_die ( "gettimeofday() failed" );

	write_rtc ( tv. tv_sec, utc );
	return 0;
}


static int check_utc(void)
{
	int utc = 0;
	FILE *f = fopen ( "/var/lib/hwclock/adjtime", "r" );

	if ( f ) {
		char buffer [128];

		while ( fgets ( buffer, sizeof( buffer ), f )) {
			int len = bb_strlen ( buffer );

			while ( len && isspace ( buffer [len - 1] ))
				len--;

			buffer [len] = 0;

			if ( strncmp ( buffer, "UTC", 3 ) == 0 ) {
				utc = 1;
				break;
			}
		}
		fclose ( f );
	}
	return utc;
}

#define HWCLOCK_OPT_LOCALTIME	1
#define HWCLOCK_OPT_UTC      	2
#define HWCLOCK_OPT_SHOW     	4
#define HWCLOCK_OPT_HCTOSYS  	8
#define HWCLOCK_OPT_SYSTOHC  	16

extern int hwclock_main ( int argc, char **argv )
{
	unsigned long opt;
	int utc;

#ifdef CONFIG_FEATURE_HWCLOCK_LONGOPTIONS
static const struct option hwclock_long_options[] = {
		{ "localtime", 0, 0, 'l' },
		{ "utc",       0, 0, 'u' },
		{ "show",      0, 0, 'r' },
		{ "hctosys",   0, 0, 's' },
		{ "systohc",   0, 0, 'w' },
		{ 0,           0, 0, 0 }
	};
	bb_applet_long_options = hwclock_long_options;
#endif

	bb_opt_complementaly = "r~ws:w~rs:s~wr:l~u:u~l";
	opt = bb_getopt_ulflags(argc, argv, "lursw");
	/* Check only one mode was given */
	if(opt & 0x80000000UL) {
		bb_show_usage();
	}

	/* If -u or -l wasn't given check if we are using utc */
	if (opt & (HWCLOCK_OPT_UTC | HWCLOCK_OPT_LOCALTIME)) 
		utc = opt & HWCLOCK_OPT_UTC;
	else
		utc = check_utc();
	
	if (opt & HWCLOCK_OPT_HCTOSYS) {
		return to_sys_clock ( utc );
	}
	else if (opt & HWCLOCK_OPT_SYSTOHC) {
		return from_sys_clock ( utc );
	} else {
		/* default HWCLOCK_OPT_SHOW */
		return show_clock ( utc );
	}
}
