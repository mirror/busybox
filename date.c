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

#include "internal.h"
#define BB_DECLARE_EXTERN
#define bb_need_invalid_date
#define bb_need_memory_exhausted
#include "messages.c"
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>


/* This 'date' command supports only 2 time setting formats, 
   all the GNU strftime stuff (its in libc, lets use it),
   setting time using UTC and displaying int, as well as
   an RFC 822 complient date output for shell scripting
   mail commands */

static const char date_usage[] = "date [OPTION]... [+FORMAT]\n"
	"  or:  date [OPTION] [MMDDhhmm[[CC]YY][.ss]]\n\n"
	"Display the current time in the given FORMAT, or set the system date.\n"
	"\nOptions:\n\t-R\t\toutput RFC-822 compliant date string\n"
	"\t-s\t\tset time described by STRING\n"

	"\t-u\t\tprint or set Coordinated Universal Time\n";


/* Input parsing code is always bulky - used heavy duty libc stuff as
   much as possible, missed out a lot of bounds checking */

/* Default input handling to save suprising some people */

struct tm *date_conv_time(struct tm *tm_time, const char *t_string)
{
	int nr;

	nr = sscanf(t_string, "%2d%2d%2d%2d%d",
				&(tm_time->tm_mon),
				&(tm_time->tm_mday),
				&(tm_time->tm_hour),
				&(tm_time->tm_min), &(tm_time->tm_year));

	if (nr < 4 || nr > 5) {
		fprintf(stderr, invalid_date, "date", t_string);
		exit(FALSE);
	}

	/* correct for century  - minor Y2K problem here? */
	if (tm_time->tm_year >= 1900)
		tm_time->tm_year -= 1900;
	/* adjust date */
	tm_time->tm_mon -= 1;

	return (tm_time);

}


/* The new stuff for LRP */

struct tm *date_conv_ftime(struct tm *tm_time, const char *t_string)
{
	struct tm itm_time, jtm_time, ktm_time, ltm_time, mtm_time, ntm_time;

	itm_time = *tm_time;
	jtm_time = *tm_time;
	ktm_time = *tm_time;
	ltm_time = *tm_time;
	mtm_time = *tm_time;
	ntm_time = *tm_time;

	/* Parse input and assign appropriately to tm_time */

	if (sscanf(t_string, "%d:%d:%d",
			   &itm_time.tm_hour, &itm_time.tm_min, &itm_time.tm_sec) == 3) {

		*tm_time = itm_time;
		return (tm_time);

	} else if (sscanf(t_string, "%d:%d",
					  &jtm_time.tm_hour, &jtm_time.tm_min) == 2) {

		*tm_time = jtm_time;
		return (tm_time);

	} else if (sscanf(t_string, "%d.%d-%d:%d:%d",
					  &ktm_time.tm_mon,
					  &ktm_time.tm_mday,
					  &ktm_time.tm_hour,
					  &ktm_time.tm_min, &ktm_time.tm_sec) == 5) {

		ktm_time.tm_mon -= 1;	/* Adjust dates from 1-12 to 0-11 */
		*tm_time = ktm_time;
		return (tm_time);

	} else if (sscanf(t_string, "%d.%d-%d:%d",
					  &ltm_time.tm_mon,
					  &ltm_time.tm_mday,
					  &ltm_time.tm_hour, &ltm_time.tm_min) == 4) {

		ltm_time.tm_mon -= 1;	/* Adjust dates from 1-12 to 0-11 */
		*tm_time = ltm_time;
		return (tm_time);

	} else if (sscanf(t_string, "%d.%d.%d-%d:%d:%d",
					  &mtm_time.tm_year,
					  &mtm_time.tm_mon,
					  &mtm_time.tm_mday,
					  &mtm_time.tm_hour,
					  &mtm_time.tm_min, &mtm_time.tm_sec) == 6) {

		mtm_time.tm_year -= 1900;	/* Adjust years */
		mtm_time.tm_mon -= 1;	/* Adjust dates from 1-12 to 0-11 */
		*tm_time = mtm_time;
		return (tm_time);

	} else if (sscanf(t_string, "%d.%d.%d-%d:%d",
					  &ntm_time.tm_year,
					  &ntm_time.tm_mon,
					  &ntm_time.tm_mday,
					  &ntm_time.tm_hour, &ntm_time.tm_min) == 5) {
		ntm_time.tm_year -= 1900;	/* Adjust years */
		ntm_time.tm_mon -= 1;	/* Adjust dates from 1-12 to 0-11 */
		*tm_time = ntm_time;
		return (tm_time);

	}

	fprintf(stderr, invalid_date, "date", t_string);

	exit(FALSE);

}


int date_main(int argc, char **argv)
{
	char *date_str = NULL;
	char *date_fmt = NULL;
	char *t_buff;
	int i;
	int set_time = 0;
	int rfc822 = 0;
	int utc = 0;
	int use_arg = 0;
	time_t tm;
	struct tm tm_time;

	/* Interpret command line args */
	i = --argc;
	argv++;
	while (i > 0 && **argv) {
		if (**argv == '-') {
			while (i > 0 && *++(*argv))
				switch (**argv) {
				case 'R':
					rfc822 = 1;
					break;
				case 's':
					set_time = 1;
					if (date_str != NULL)
						usage(date_usage);
					date_str = optarg;
					break;
				case 'u':
					utc = 1;
					if (putenv("TZ=UTC0") != 0) {
						fprintf(stderr, memory_exhausted, "date");
						exit(FALSE);
					}
					/* Look ma, no break.  Don't fix it either. */
				case 'd':
					use_arg = 1;
					if (date_str != NULL)
						usage(date_usage);
					date_str = optarg;
					break;
				case '-':
					usage(date_usage);
				}
		} else {
			if ((date_fmt == NULL) && (strcmp(*argv, "+") == 0))
				date_fmt = *argv;
			else if (date_str == NULL) {
				set_time = 1;
				date_str = *argv;
			} else {
				usage(date_usage);
			}
		}
		i--;
		argv++;
	}


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

		/* Correct any day of week and day of year etc fields */
		tm = mktime(&tm_time);
		if (tm < 0) {
			fprintf(stderr, invalid_date, "date", date_str);
			exit(FALSE);
		}

		/* if setting time, set it */
		if (set_time) {
			if (stime(&tm) < 0) {
				fprintf(stderr, "date: can't set date.\n");
				exit(FALSE);
			}
		}
	}

	/* Display output */

	/* Deal with format string */
	if (date_fmt == NULL) {
		date_fmt = (rfc822
					? (utc
					   ? "%a, %_d %b %Y %H:%M:%S GMT"
					   : "%a, %_d %b %Y %H:%M:%S %z")
					: "%a %b %e %H:%M:%S %Z %Y");

	} else if (*date_fmt == '\0') {
		/* Imitate what GNU 'date' does with NO format string! */
		printf("\n");
		exit(TRUE);
	}

	/* Handle special conversions */

	if (strncmp(date_fmt, "%f", 2) == 0) {
		date_fmt = "%Y.%m.%d-%H:%M:%S";
	}

	/* Print OUTPUT (after ALL that!) */
	t_buff = xmalloc(201);
	strftime(t_buff, 200, date_fmt, &tm_time);
	printf("%s\n", t_buff);

	exit(TRUE);

}
