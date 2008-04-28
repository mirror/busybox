/* vi: set sw=4 ts=4: */
/*
 * Mini date implementation for busybox
 *
 * by Matthew Grant <grantma@anathoth.gen.nz>
 *
 * iso-format handling added by Robert Griebl <griebl@gmx.de>
 * bugfixes and cleanup by Bernhard Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
*/

#include "libbb.h"

/* This 'date' command supports only 2 time setting formats,
   all the GNU strftime stuff (its in libc, lets use it),
   setting time using UTC and displaying it, as well as
   an RFC 2822 compliant date output for shell scripting
   mail commands */

/* Input parsing code is always bulky - used heavy duty libc stuff as
   much as possible, missed out a lot of bounds checking */

/* Default input handling to save surprising some people */


#define DATE_OPT_RFC2822	0x01
#define DATE_OPT_SET		0x02
#define DATE_OPT_UTC		0x04
#define DATE_OPT_DATE		0x08
#define DATE_OPT_REFERENCE	0x10
#define DATE_OPT_TIMESPEC	0x20
#define DATE_OPT_HINT		0x40

static void maybe_set_utc(int opt)
{
	if (opt & DATE_OPT_UTC)
		putenv((char*)"TZ=UTC0");
}

int date_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int date_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	struct tm tm_time;
	time_t tm;
	unsigned opt;
	int ifmt = -1;
	char *date_str;
	char *fmt_dt2str;
	char *fmt_str2dt;
	char *filename;
	char *isofmt_arg = NULL;

	opt_complementary = "d--s:s--d"
		USE_FEATURE_DATE_ISOFMT(":R--I:I--R");
	opt = getopt32(argv, "Rs:ud:r:"
			USE_FEATURE_DATE_ISOFMT("I::D:"),
			&date_str, &date_str, &filename
			USE_FEATURE_DATE_ISOFMT(, &isofmt_arg, &fmt_str2dt));
	argv += optind;
	maybe_set_utc(opt);

	if (ENABLE_FEATURE_DATE_ISOFMT && (opt & DATE_OPT_TIMESPEC)) {
		ifmt = 0; /* default is date */
		if (isofmt_arg) {
			static const char isoformats[] ALIGN1 =
				"date\0""hours\0""minutes\0""seconds\0";
			ifmt = index_in_strings(isoformats, isofmt_arg);
			if (ifmt < 0)
				bb_show_usage();
		}
	}

	fmt_dt2str = NULL;
	if (argv[0] && argv[0][0] == '+') {
		fmt_dt2str = &argv[0][1];	/* Skip over the '+' */
		argv++;
	}
	if (!(opt & (DATE_OPT_SET | DATE_OPT_DATE))) {
		opt |= DATE_OPT_SET;
		date_str = argv[0]; /* can be NULL */
		if (date_str)
			argv++;
	}
	if (*argv)
		bb_show_usage();

	/* Now we have parsed all the information except the date format
	   which depends on whether the clock is being set or read */

	if (opt & DATE_OPT_REFERENCE) {
		struct stat statbuf;
		xstat(filename, &statbuf);
		tm = statbuf.st_mtime;
	} else
		time(&tm);
	memcpy(&tm_time, localtime(&tm), sizeof(tm_time));

	/* If date string is given, update tm_time, and maybe set date */
	if (date_str != NULL) {
		/* Zero out fields - take her back to midnight! */
		tm_time.tm_sec = 0;
		tm_time.tm_min = 0;
		tm_time.tm_hour = 0;

		/* Process any date input to UNIX time since 1 Jan 1970 */
		if (ENABLE_FEATURE_DATE_ISOFMT && (opt & DATE_OPT_HINT)) {
			if (strptime(date_str, fmt_str2dt, &tm_time) == NULL)
				bb_error_msg_and_die(bb_msg_invalid_date, date_str);
		} else {
			char end = '\0';
			const char *last_colon = strrchr(date_str, ':');

			if (last_colon != NULL) {
				/* Parse input and assign appropriately to tm_time */

				if (sscanf(date_str, "%u:%u%c",
								&tm_time.tm_hour,
								&tm_time.tm_min,
								&end) >= 2) {
					/* no adjustments needed */
				} else if (sscanf(date_str, "%u.%u-%u:%u%c",
								&tm_time.tm_mon, &tm_time.tm_mday,
								&tm_time.tm_hour, &tm_time.tm_min,
								&end) >= 4) {
					/* Adjust dates from 1-12 to 0-11 */
					tm_time.tm_mon -= 1;
				} else if (sscanf(date_str, "%u.%u.%u-%u:%u%c", &tm_time.tm_year,
								&tm_time.tm_mon, &tm_time.tm_mday,
								&tm_time.tm_hour, &tm_time.tm_min,
								&end) >= 5) {
					tm_time.tm_year -= 1900; /* Adjust years */
					tm_time.tm_mon -= 1; /* Adjust dates from 1-12 to 0-11 */
				} else if (sscanf(date_str, "%u-%u-%u %u:%u%c", &tm_time.tm_year,
								&tm_time.tm_mon, &tm_time.tm_mday,
								&tm_time.tm_hour, &tm_time.tm_min,
								&end) >= 5) {
					tm_time.tm_year -= 1900; /* Adjust years */
					tm_time.tm_mon -= 1; /* Adjust dates from 1-12 to 0-11 */
//TODO: coreutils 6.9 also accepts "YYYY-MM-DD HH" (no minutes)
				} else {
					bb_error_msg_and_die(bb_msg_invalid_date, date_str);
				}
				if (end == ':') {
					if (sscanf(last_colon + 1, "%u%c", &tm_time.tm_sec, &end) == 1)
						end = '\0';
					/* else end != NUL and we error out */
				}
			} else {
				if (sscanf(date_str, "%2u%2u%2u%2u%u%c", &tm_time.tm_mon,
							&tm_time.tm_mday, &tm_time.tm_hour, &tm_time.tm_min,
							&tm_time.tm_year, &end) < 4)
					bb_error_msg_and_die(bb_msg_invalid_date, date_str);
				/* correct for century  - minor Y2K problem here? */
				if (tm_time.tm_year >= 1900) {
					tm_time.tm_year -= 1900;
				}
				/* adjust date */
				tm_time.tm_mon -= 1;
				if (end == '.') {
					if (sscanf(strchr(date_str, '.') + 1, "%u%c",
							&tm_time.tm_sec, &end) == 1)
						end = '\0';
					/* else end != NUL and we error out */
				}
			}
			if (end != '\0') {
				bb_error_msg_and_die(bb_msg_invalid_date, date_str);
			}
		}
		/* Correct any day of week and day of year etc. fields */
		tm_time.tm_isdst = -1;	/* Be sure to recheck dst. */
		tm = mktime(&tm_time);
		if (tm < 0) {
			bb_error_msg_and_die(bb_msg_invalid_date, date_str);
		}
		maybe_set_utc(opt);

		/* if setting time, set it */
		if ((opt & DATE_OPT_SET) && stime(&tm) < 0) {
			bb_perror_msg("cannot set date");
		}
	}

	/* Display output */

	/* Deal with format string */
	if (fmt_dt2str == NULL) {
		int i;
		fmt_dt2str = xzalloc(32);
		if (ENABLE_FEATURE_DATE_ISOFMT && ifmt >= 0) {
			strcpy(fmt_dt2str, "%Y-%m-%d");
			if (ifmt > 0) {
				i = 8;
				fmt_dt2str[i++] = 'T';
				fmt_dt2str[i++] = '%';
				fmt_dt2str[i++] = 'H';
				if (ifmt > 1) {
					fmt_dt2str[i++] = ':';
					fmt_dt2str[i++] = '%';
					fmt_dt2str[i++] = 'M';
					if (ifmt > 2) {
						fmt_dt2str[i++] = ':';
						fmt_dt2str[i++] = '%';
						fmt_dt2str[i++] = 'S';
					}
				}
 format_utc:
				fmt_dt2str[i++] = '%';
				fmt_dt2str[i] = (opt & DATE_OPT_UTC) ? 'Z' : 'z';
			}
		} else if (opt & DATE_OPT_RFC2822) {
			/* Undo busybox.c for date -R */
			if (ENABLE_LOCALE_SUPPORT)
				setlocale(LC_TIME, "C");
			strcpy(fmt_dt2str, "%a, %d %b %Y %H:%M:%S ");
			i = 22;
			goto format_utc;
		} else /* default case */
			fmt_dt2str = (char*)"%a %b %e %H:%M:%S %Z %Y";
	}

#define date_buf bb_common_bufsiz1
	if (*fmt_dt2str == '\0') {
		/* With no format string, just print a blank line */
		date_buf[0] = '\0';
	} else {
		/* Handle special conversions */
		if (strncmp(fmt_dt2str, "%f", 2) == 0) {
			fmt_dt2str = (char*)"%Y.%m.%d-%H:%M:%S";
		}

		/* Generate output string */
		strftime(date_buf, sizeof(date_buf), fmt_dt2str, &tm_time);
	}
	puts(date_buf);

	return EXIT_SUCCESS;
}
