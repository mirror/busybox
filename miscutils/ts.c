/* vi: set sw=4 ts=4: */
/*
 * Copyright (C) 2019 Denys Vlasenko <vda.linux@googlemail.com>
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */
//config:config TS
//config:	bool "ts (450 bytes)"
//config:	default y

//applet:IF_TS(APPLET(ts, BB_DIR_USR_BIN, BB_SUID_DROP))

//kbuild:lib-$(CONFIG_TS) += ts.o

//usage:#define ts_trivial_usage
//usage:       "[-is] [STRFTIME]"
//usage:#define ts_full_usage ""

#include "libbb.h"
#include "common_bufsiz.h"
# include <sys/syscall.h>

int ts_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int ts_main(int argc UNUSED_PARAM, char **argv)
{
	struct timespec base;
	unsigned opt;
	char *frac;
	char *fmt_dt2str;
	char *line;

	opt = getopt32(argv, "^" "is" "\0" "?1" /*max one arg*/);
	if (opt) {
		putenv((char*)"TZ=UTC0");
		tzset();
	}
	/*argc -= optind;*/
	argv += optind;
	fmt_dt2str = argv[0] ? argv[0]
		: (char*)(opt ? "%b %d %H:%M:%S"+6 : "%b %d %H:%M:%S");
	frac = is_suffixed_with(fmt_dt2str, "%.S");
	if (!frac)
		frac = is_suffixed_with(fmt_dt2str, "%.s");
	if (frac) {
		frac++;
		strcpy(frac, frac + 1);
	}

#define date_buf bb_common_bufsiz1
	setup_common_bufsiz();
	syscall(__NR_clock_gettime, CLOCK_REALTIME, &base);

	while ((line = xmalloc_fgets(stdin)) != NULL) {
		struct timespec ts;
		struct tm tm_time;

		/* libc has incredibly messy way of doing this,
		 * typically requiring -lrt. We just skip all this mess
		 */
		syscall(__NR_clock_gettime, CLOCK_REALTIME, &ts);
		if (opt) {
			/* -i and/or -s */
			struct timespec ts1 = ts1;
			if (opt & 1) /* -i */
				ts1 = ts;
//printf("%d %d\n", ts.tv_sec, base.tv_sec);
			ts.tv_sec -= base.tv_sec;
//printf("%d %d\n", ts.tv_sec, base.tv_sec);
			ts.tv_nsec -= base.tv_nsec;
			if ((int32_t)(ts.tv_nsec) < 0) {
				ts.tv_sec--;
				ts.tv_nsec += 1000*1000*1000;
			}
			if (opt & 1) /* -i */
				base = ts1;
			localtime_r(&ts.tv_sec, &tm_time);
		}
		strftime(date_buf, COMMON_BUFSIZE, fmt_dt2str, &tm_time);
		if (!frac) {
			printf("%s %s", date_buf, line);
		} else {
			printf("%s.%06u %s", date_buf, (unsigned)ts.tv_nsec / 1000u, line);
		}
		free(line);
	}

	return EXIT_SUCCESS;
}
