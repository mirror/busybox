/* vi: set sw=4 ts=4: */
/*
 * Mini date implementation for busybox
 *
 * by Matthew Grant <grantma@anathoth.gen.nz>
 *
 * iso-format handling added by Robert Griebl <griebl@gmx.de>
 * bugfixes and cleanup by Bernhard Reutner-Fischer
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
*/

/* This 'date' command supports only 2 time setting formats,
   all the GNU strftime stuff (its in libc, lets use it),
   setting time using UTC and displaying it, as well as
   an RFC 2822 compliant date output for shell scripting
   mail commands */

/* Input parsing code is always bulky - used heavy duty libc stuff as
   much as possible, missed out a lot of bounds checking */

/* Default input handling to save surprising some people */

/* GNU coreutils 6.9 man page:
 * date [OPTION]... [+FORMAT]
 * date [-u|--utc|--universal] [MMDDhhmm[[CC]YY][.ss]]
 * -d, --date=STRING
 *      display time described by STRING, not `now'
 * -f, --file=DATEFILE
 *      like --date once for each line of DATEFILE
 * -r, --reference=FILE
 *      display the last modification time of FILE
 * -R, --rfc-2822
 *      output date and time in RFC 2822 format.
 *      Example: Mon, 07 Aug 2006 12:34:56 -0600
 * --rfc-3339=TIMESPEC
 *      output date and time in RFC 3339 format.
 *      TIMESPEC='date', 'seconds', or 'ns'
 *      Date and time components are separated by a single space:
 *      2006-08-07 12:34:56-06:00
 * -s, --set=STRING
 *      set time described by STRING
 * -u, --utc, --universal
 *      print or set Coordinated Universal Time
 *
 * Busybox:
 * long options are not supported
 * -f is not supported
 * -I seems to roughly match --rfc-3339, but -I has _optional_ param
 *    (thus "-I seconds" doesn't work, only "-Iseconds"),
 *    and does not support -Ins
 * -D FMT is a bbox extension for _input_ conversion of -d DATE
 */
#include "libbb.h"

enum {
	OPT_RFC2822   = (1 << 0), /* R */
	OPT_SET       = (1 << 1), /* s */
	OPT_UTC       = (1 << 2), /* u */
	OPT_DATE      = (1 << 3), /* d */
	OPT_REFERENCE = (1 << 4), /* r */
	OPT_TIMESPEC  = (1 << 5) * ENABLE_FEATURE_DATE_ISOFMT, /* I */
	OPT_HINT      = (1 << 6) * ENABLE_FEATURE_DATE_ISOFMT, /* D */
};

static void maybe_set_utc(int opt)
{
	if (opt & OPT_UTC)
		putenv((char*)"TZ=UTC0");
}

#if ENABLE_LONG_OPTS
static const char date_longopts[] ALIGN1 =
		"rfc-822\0"   No_argument       "R"
		"rfc-2822\0"  No_argument       "R"
		"set\0"       Required_argument "s"
		"utc\0"       No_argument       "u"
	/*	"universal\0" No_argument       "u" */
		"date\0"      Required_argument "d"
		"reference\0" Required_argument "r"
		;
#endif

int date_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int date_main(int argc UNUSED_PARAM, char **argv)
{
	struct tm tm_time;
	char buf_fmt_dt2str[64];
	time_t tm;
	unsigned opt;
	int ifmt = -1;
	char *date_str;
	char *fmt_dt2str;
	char *fmt_str2dt;
	char *filename;
	char *isofmt_arg = NULL;

	opt_complementary = "d--s:s--d"
		IF_FEATURE_DATE_ISOFMT(":R--I:I--R");
	IF_LONG_OPTS(applet_long_options = date_longopts;)
	opt = getopt32(argv, "Rs:ud:r:"
			IF_FEATURE_DATE_ISOFMT("I::D:"),
			&date_str, &date_str, &filename
			IF_FEATURE_DATE_ISOFMT(, &isofmt_arg, &fmt_str2dt));
	argv += optind;
	maybe_set_utc(opt);

	if (ENABLE_FEATURE_DATE_ISOFMT && (opt & OPT_TIMESPEC)) {
		ifmt = 0; /* default is date */
		if (isofmt_arg) {
			static const char isoformats[] ALIGN1 =
				"date\0""hours\0""minutes\0""seconds\0"; /* ns? */
			ifmt = index_in_substrings(isoformats, isofmt_arg);
			if (ifmt < 0)
				bb_show_usage();
		}
	}

	fmt_dt2str = NULL;
	if (argv[0] && argv[0][0] == '+') {
		fmt_dt2str = &argv[0][1]; /* skip over the '+' */
		argv++;
	}
	if (!(opt & (OPT_SET | OPT_DATE))) {
		opt |= OPT_SET;
		date_str = argv[0]; /* can be NULL */
		if (date_str) {
#if ENABLE_FEATURE_DATE_COMPAT
			int len = strspn(date_str, "0123456789");
			if (date_str[len] == '\0'
			 || (date_str[len] == '.'
			    && isdigit(date_str[len+1])
			    && isdigit(date_str[len+2])
			    && date_str[len+3] == '\0'
			    )
			) {
				/* Dreaded MMDDhhmm[[CC]YY][.ss] format!
				 * It does not match -d or -s format.
				 * Some users actually do use it.
				 */
				len -= 8;
				if (len < 0 || len > 4 || (len & 1))
					bb_error_msg_and_die(bb_msg_invalid_date, date_str);
				if (len != 0) { /* move YY or CCYY to front */
					char buf[4];
					memcpy(buf, date_str + 8, len);
					memmove(date_str + len, date_str, 8);
					memcpy(date_str, buf, len);
				}
			}
#endif
			argv++;
		}
	}
	if (*argv)
		bb_show_usage();

	/* Now we have parsed all the information except the date format
	 * which depends on whether the clock is being set or read */

	if (opt & OPT_REFERENCE) {
		struct stat statbuf;
		xstat(filename, &statbuf);
		tm = statbuf.st_mtime;
	} else {
		time(&tm);
	}
	localtime_r(&tm, &tm_time);

	/* If date string is given, update tm_time, and maybe set date */
	if (date_str != NULL) {
		/* Zero out fields - take her back to midnight! */
		tm_time.tm_sec = 0;
		tm_time.tm_min = 0;
		tm_time.tm_hour = 0;

		/* Process any date input to UNIX time since 1 Jan 1970 */
		if (ENABLE_FEATURE_DATE_ISOFMT && (opt & OPT_HINT)) {
			if (strptime(date_str, fmt_str2dt, &tm_time) == NULL)
				bb_error_msg_and_die(bb_msg_invalid_date, date_str);
		} else {
			parse_datestr(date_str, &tm_time);
		}

		/* Correct any day of week and day of year etc. fields */
		tm_time.tm_isdst = -1;	/* Be sure to recheck dst */
		tm = validate_tm_time(date_str, &tm_time);

		maybe_set_utc(opt);

		/* if setting time, set it */
		if ((opt & OPT_SET) && stime(&tm) < 0) {
			bb_perror_msg("can't set date");
		}
	}

	/* Display output */

	/* Deal with format string */
	if (fmt_dt2str == NULL) {
		int i;
		fmt_dt2str = buf_fmt_dt2str;
		if (ENABLE_FEATURE_DATE_ISOFMT && ifmt >= 0) {
			/* -I[SPEC]: 0:date 1:hours 2:minutes 3:seconds */
			strcpy(fmt_dt2str, "%Y-%m-%dT%H:%M:%S");
			i = 8 + 3 * ifmt;
			if (ifmt != 0) {
				/* TODO: if (ifmt==4) i += sprintf(&fmt_dt2str[i], ",%09u", nanoseconds); */
 format_utc:
				fmt_dt2str[i++] = '%';
				fmt_dt2str[i++] = (opt & OPT_UTC) ? 'Z' : 'z';
			}
			fmt_dt2str[i] = '\0';
		} else if (opt & OPT_RFC2822) {
			/* -R. undo busybox.c setlocale */
			if (ENABLE_LOCALE_SUPPORT)
				setlocale(LC_TIME, "C");
			strcpy(fmt_dt2str, "%a, %d %b %Y %H:%M:%S ");
			i = sizeof("%a, %d %b %Y %H:%M:%S ")-1;
			goto format_utc;
		} else { /* default case */
			fmt_dt2str = (char*)"%a %b %e %H:%M:%S %Z %Y";
		}
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
