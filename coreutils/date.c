/* vi: set sw=4 ts=4: */
/*
 * Mini date implementation for busybox
 *
 * by Matthew Grant <grantma@anathoth.gen.nz>
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

#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include "busybox.h"


/* This 'date' command supports only 2 time setting formats, 
   all the GNU strftime stuff (its in libc, lets use it),
   setting time using UTC and displaying int, as well as
   an RFC 822 complient date output for shell scripting
   mail commands */

/* Input parsing code is always bulky - used heavy duty libc stuff as
   much as possible, missed out a lot of bounds checking */

/* Default input handling to save suprising some people */

static struct tm *date_conv_time(struct tm *tm_time, const char *t_string)
{
	int nr;

	nr = sscanf(t_string, "%2d%2d%2d%2d%d",
				&(tm_time->tm_mon),
				&(tm_time->tm_mday),
				&(tm_time->tm_hour),
				&(tm_time->tm_min), &(tm_time->tm_year));

	if (nr < 4 || nr > 5) {
		error_msg_and_die(invalid_date, t_string); 
	}

	/* correct for century  - minor Y2K problem here? */
	if (tm_time->tm_year >= 1900)
		tm_time->tm_year -= 1900;
	/* adjust date */
	tm_time->tm_mon -= 1;

	return (tm_time);

}


/* The new stuff for LRP */

static struct tm *date_conv_ftime(struct tm *tm_time, const char *t_string)
{
	struct tm t;

	/* Parse input and assign appropriately to tm_time */

	if (t=*tm_time,sscanf(t_string, "%d:%d:%d",
			   &t.tm_hour, &t.tm_min, &t.tm_sec) == 3) {
					/* no adjustments needed */

	} else if (t=*tm_time,sscanf(t_string, "%d:%d",
					  &t.tm_hour, &t.tm_min) == 2) {
					/* no adjustments needed */


	} else if (t=*tm_time,sscanf(t_string, "%d.%d-%d:%d:%d",
					  &t.tm_mon,
					  &t.tm_mday,
					  &t.tm_hour,
					  &t.tm_min, &t.tm_sec) == 5) {

		t.tm_mon -= 1;	/* Adjust dates from 1-12 to 0-11 */

	} else if (t=*tm_time,sscanf(t_string, "%d.%d-%d:%d",
					  &t.tm_mon,
					  &t.tm_mday,
					  &t.tm_hour, &t.tm_min) == 4) {

		t.tm_mon -= 1;	/* Adjust dates from 1-12 to 0-11 */

	} else if (t=*tm_time,sscanf(t_string, "%d.%d.%d-%d:%d:%d",
					  &t.tm_year,
					  &t.tm_mon,
					  &t.tm_mday,
					  &t.tm_hour,
					  &t.tm_min, &t.tm_sec) == 6) {

		t.tm_year -= 1900;	/* Adjust years */
		t.tm_mon -= 1;	/* Adjust dates from 1-12 to 0-11 */

	} else if (t=*tm_time,sscanf(t_string, "%d.%d.%d-%d:%d",
					  &t.tm_year,
					  &t.tm_mon,
					  &t.tm_mday,
					  &t.tm_hour, &t.tm_min) == 5) {
		t.tm_year -= 1900;	/* Adjust years */
		t.tm_mon -= 1;	/* Adjust dates from 1-12 to 0-11 */

	} else {
		error_msg_and_die(invalid_date, t_string); 
	}
	*tm_time = t;
	return (tm_time);
}


int date_main(int argc, char **argv)
{
	char *date_str = NULL;
	char *date_fmt = NULL;
	char *t_buff;
	int c;
	int set_time = 0;
	int rfc822 = 0;
	int utc = 0;
	int use_arg = 0;
	time_t tm;
	struct tm tm_time;

	/* Interpret command line args */
	while ((c = getopt(argc, argv, "Rs:ud:")) != EOF) {
		switch (c) {
			case 'R':
				rfc822 = 1;
				break;
			case 's':
				set_time = 1;
				if ((date_str != NULL) || ((date_str = optarg) == NULL)) {
					show_usage();
				}
				break;
			case 'u':
				utc = 1;
				if (putenv("TZ=UTC0") != 0)
					error_msg_and_die(memory_exhausted);
				break;
			case 'd':
				use_arg = 1;
				if ((date_str != NULL) || ((date_str = optarg) == NULL))
					show_usage();
				break;
			default:
				show_usage();
		}
	}

	if ((date_fmt == NULL) && (optind < argc) && (argv[optind][0] == '+'))
		date_fmt = &argv[optind][1];   /* Skip over the '+' */
	else if (date_str == NULL) {
		set_time = 1;
		date_str = argv[optind];
	} 
#if 0
	else {
		error_msg("date_str='%s'  date_fmt='%s'\n", date_str, date_fmt);
		show_usage();
	}
#endif

	/* Now we have parsed all the information except the date format
	   which depends on whether the clock is being set or read */

	time(&tm);
	memcpy(&tm_time, localtime(&tm), sizeof(tm_time));
	/* Zero out fields - take her back to midnight! */
	if (date_str != NULL) {
		tm_time.tm_sec = 0;
		tm_time.tm_min = 0;
		tm_time.tm_hour = 0;
	}

	/* Process any date input to UNIX time since 1 Jan 1970 */
	if (date_str != NULL) {

		if (strchr(date_str, ':') != NULL) {
			date_conv_ftime(&tm_time, date_str);
		} else {
			date_conv_time(&tm_time, date_str);
		}

		/* Correct any day of week and day of year etc. fields */
		tm = mktime(&tm_time);
		if (tm < 0)
			error_msg_and_die(invalid_date, date_str); 
		if ( utc ) {
			if (putenv("TZ=UTC0") != 0)
				error_msg_and_die(memory_exhausted);
		}

		/* if setting time, set it */
		if (set_time) {
			if (stime(&tm) < 0) {
				perror_msg("cannot set date");
			}
		}
	}

	/* Display output */

	/* Deal with format string */
	if (date_fmt == NULL) {
		date_fmt = (rfc822
					? (utc
					   ? "%a, %e %b %Y %H:%M:%S GMT"
					   : "%a, %e %b %Y %H:%M:%S %z")
					: "%a %b %e %H:%M:%S %Z %Y");

	} else if (*date_fmt == '\0') {
		/* Imitate what GNU 'date' does with NO format string! */
		printf("\n");
		return EXIT_SUCCESS;
	}

	/* Handle special conversions */

	if (strncmp(date_fmt, "%f", 2) == 0) {
		date_fmt = "%Y.%m.%d-%H:%M:%S";
	}

	/* Print OUTPUT (after ALL that!) */
	t_buff = xmalloc(201);
	strftime(t_buff, 200, date_fmt, &tm_time);
	puts(t_buff);

	return EXIT_SUCCESS;
}
