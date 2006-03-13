/* vi: set sw=4 ts=4: */
/*
 * Mini date implementation for busybox
 *
 * by Matthew Grant <grantma@anathoth.gen.nz>
 *
 * iso-format handling added by Robert Griebl <griebl@gmx.de>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
*/

#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include "busybox.h"

/* This 'date' command supports only 2 time setting formats,
   all the GNU strftime stuff (its in libc, lets use it),
   setting time using UTC and displaying int, as well as
   an RFC 822 complient date output for shell scripting
   mail commands */

/* Input parsing code is always bulky - used heavy duty libc stuff as
   much as possible, missed out a lot of bounds checking */

/* Default input handling to save surprising some people */

static struct tm *date_conv_time(struct tm *tm_time, const char *t_string)
{
	int nr;
	char *cp;

	nr = sscanf(t_string, "%2d%2d%2d%2d%d", &(tm_time->tm_mon),
				&(tm_time->tm_mday), &(tm_time->tm_hour), &(tm_time->tm_min),
				&(tm_time->tm_year));

	if (nr < 4 || nr > 5) {
		bb_error_msg_and_die(bb_msg_invalid_date, t_string);
	}

	cp = strchr(t_string, '.');
	if (cp) {
		nr = sscanf(cp + 1, "%2d", &(tm_time->tm_sec));
		if (nr != 1) {
			bb_error_msg_and_die(bb_msg_invalid_date, t_string);
		}
	}

	/* correct for century  - minor Y2K problem here? */
	if (tm_time->tm_year >= 1900) {
		tm_time->tm_year -= 1900;
	}
	/* adjust date */
	tm_time->tm_mon -= 1;

	return (tm_time);

}


/* The new stuff for LRP */

static struct tm *date_conv_ftime(struct tm *tm_time, const char *t_string)
{
	struct tm t;

	/* Parse input and assign appropriately to tm_time */

	if (t = *tm_time, sscanf(t_string, "%d:%d:%d", &t.tm_hour, &t.tm_min,
						 &t.tm_sec) == 3) {
		/* no adjustments needed */
	} else if (t = *tm_time, sscanf(t_string, "%d:%d", &t.tm_hour,
								&t.tm_min) == 2) {
		/* no adjustments needed */
	} else if (t = *tm_time, sscanf(t_string, "%d.%d-%d:%d:%d", &t.tm_mon,
						&t.tm_mday, &t.tm_hour,
						&t.tm_min, &t.tm_sec) == 5) {
		/* Adjust dates from 1-12 to 0-11 */
		t.tm_mon -= 1;
	} else if (t = *tm_time, sscanf(t_string, "%d.%d-%d:%d", &t.tm_mon,
						&t.tm_mday,
						&t.tm_hour, &t.tm_min) == 4) {
		/* Adjust dates from 1-12 to 0-11 */
		t.tm_mon -= 1;
	} else if (t = *tm_time, sscanf(t_string, "%d.%d.%d-%d:%d:%d", &t.tm_year,
						&t.tm_mon, &t.tm_mday,
						&t.tm_hour, &t.tm_min,
							&t.tm_sec) == 6) {
		t.tm_year -= 1900;	/* Adjust years */
		t.tm_mon -= 1;	/* Adjust dates from 1-12 to 0-11 */
	} else if (t = *tm_time, sscanf(t_string, "%d.%d.%d-%d:%d", &t.tm_year,
						&t.tm_mon, &t.tm_mday,
						&t.tm_hour, &t.tm_min) == 5) {
		t.tm_year -= 1900;	/* Adjust years */
		t.tm_mon -= 1;	/* Adjust dates from 1-12 to 0-11 */
	} else {
		bb_error_msg_and_die(bb_msg_invalid_date, t_string);
	}
	*tm_time = t;
	return (tm_time);
}

#define DATE_OPT_RFC2822	0x01
#define DATE_OPT_SET		0x02
#define DATE_OPT_UTC		0x04
#define DATE_OPT_DATE		0x08
#define DATE_OPT_REFERENCE	0x10
#define DATE_OPT_TIMESPEC	0x20
#define DATE_OPT_HINT		0x40

int date_main(int argc, char **argv)
{
	char *date_str = NULL;
	char *date_fmt = NULL;
	int set_time;
	int utc;
	time_t tm;
	unsigned long opt;
	struct tm tm_time;
	char *filename = NULL;

	int ifmt = 0;
	char *isofmt_arg;
	char *hintfmt_arg;

	bb_opt_complementally = "?:d--s:s--d";
	opt = bb_getopt_ulflags(argc, argv, "Rs:ud:r:"
		  		 	USE_FEATURE_DATE_ISOFMT("I::D:"),
					&date_str, &date_str, &filename
					USE_FEATURE_DATE_ISOFMT(, &isofmt_arg, &hintfmt_arg));
	set_time = opt & DATE_OPT_SET;
	utc = opt & DATE_OPT_UTC;
	if (utc && putenv("TZ=UTC0") != 0) {
		bb_error_msg_and_die(bb_msg_memory_exhausted);
	}

	if(ENABLE_FEATURE_DATE_ISOFMT && (opt & DATE_OPT_TIMESPEC)) {
		if (!isofmt_arg) {
			ifmt = 1;
		} else {
			char *isoformats[]={"date","hours","minutes","seconds"};
			for(ifmt = 4; ifmt;)
				if(!strcmp(isofmt_arg,isoformats[--ifmt]))
					break;
		}
		if (!ifmt) {
			bb_show_usage();
		}
	}

	/* XXX, date_fmt == NULL from this always */
	if ((date_fmt == NULL) && (optind < argc) && (argv[optind][0] == '+')) {
		date_fmt = &argv[optind][1];	/* Skip over the '+' */
	} else if (date_str == NULL) {
		set_time = 1;
		date_str = argv[optind];
	}

	/* Now we have parsed all the information except the date format
	   which depends on whether the clock is being set or read */

	if(filename) {
		struct stat statbuf;
		xstat(filename,&statbuf);
		tm=statbuf.st_mtime;
	} else time(&tm);
	memcpy(&tm_time, localtime(&tm), sizeof(tm_time));
	/* Zero out fields - take her back to midnight! */
	if (date_str != NULL) {
		tm_time.tm_sec = 0;
		tm_time.tm_min = 0;
		tm_time.tm_hour = 0;

		/* Process any date input to UNIX time since 1 Jan 1970 */
		if (ENABLE_FEATURE_DATE_ISOFMT && (opt & DATE_OPT_HINT)) {
			strptime(date_str, hintfmt_arg, &tm_time);
		} else if (strchr(date_str, ':') != NULL) {
			date_conv_ftime(&tm_time, date_str);
		} else {
			date_conv_time(&tm_time, date_str);
		}

		/* Correct any day of week and day of year etc. fields */
		tm_time.tm_isdst = -1;	/* Be sure to recheck dst. */
		tm = mktime(&tm_time);
		if (tm < 0) {
			bb_error_msg_and_die(bb_msg_invalid_date, date_str);
		}
		if (utc && putenv("TZ=UTC0") != 0) {
			bb_error_msg_and_die(bb_msg_memory_exhausted);
		}

		/* if setting time, set it */
		if (set_time && stime(&tm) < 0) {
			bb_perror_msg("cannot set date");
		}
	}

	/* Display output */

	/* Deal with format string */
	if (date_fmt == NULL) {
		/* Start with the default case */
		
		date_fmt = (opt & DATE_OPT_RFC2822 ?
					(utc ? "%a, %d %b %Y %H:%M:%S GMT" :
					"%a, %d %b %Y %H:%M:%S %z") :
					"%a %b %e %H:%M:%S %Z %Y");

		if (ENABLE_FEATURE_DATE_ISOFMT) {
			if (ifmt == 4)
				date_fmt = utc ? "%Y-%m-%dT%H:%M:%SZ" : "%Y-%m-%dT%H:%M:%S%z";
			else if (ifmt == 3)
				date_fmt = utc ? "%Y-%m-%dT%H:%MZ" : "%Y-%m-%dT%H:%M%z";
			else if (ifmt == 2) 
				date_fmt = utc ? "%Y-%m-%dT%HZ" : "%Y-%m-%dT%H%z";
			else if (ifmt == 1)
				date_fmt = "%Y-%m-%d";
		}
	}
	
	if (*date_fmt == '\0') {

		/* With no format string, just print a blank line */
		
		*bb_common_bufsiz1=0;
	} else {

		/* Handle special conversions */

		if (strncmp(date_fmt, "%f", 2) == 0) {
			date_fmt = "%Y.%m.%d-%H:%M:%S";
		}

		/* Generate output string */
		strftime(bb_common_bufsiz1, 200, date_fmt, &tm_time);
	}
	puts(bb_common_bufsiz1);

	return EXIT_SUCCESS;
}
