/* vi: set sw=4 ts=4: */
/*
 * head implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
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

/* BB_AUDIT SUSv3 compliant */
/* BB_AUDIT GNU compatible -c, -q, and -v options in 'fancy' configuration. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/head.html */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>
#include <unistd.h>
#include "busybox.h"

static const char head_opts[] =
	"n:"
#ifdef CONFIG_FEATURE_FANCY_HEAD
	"c:qv"
#endif
	;

static const char header_fmt_str[] = "\n==> %s <==\n";

int head_main(int argc, char **argv)
{
	unsigned long count = 10;
	unsigned long i;
#ifdef CONFIG_FEATURE_FANCY_HEAD
	int count_bytes = 0;
	int header_threshhold = 1;
#endif

	FILE *fp;
	const char *fmt;
	char *p;
	int opt;
	int c;
	int retval = EXIT_SUCCESS;

	/* Allow legacy syntax of an initial numeric option without -n. */
	if ((argc > 1) && (argv[1][0] == '-')
		/* && (isdigit)(argv[1][1]) */
		&& (((unsigned int)(argv[1][1] - '0')) <= 9)
	) {
		--argc;
		++argv;
		p = (*argv) + 1;
		goto GET_COUNT;
	}

	while ((opt = getopt(argc, argv, head_opts)) > 0) {
		switch(opt) {
#ifdef CONFIG_FEATURE_FANCY_HEAD
			case 'q':
				header_threshhold = INT_MAX;
				break;
			case 'v':
				header_threshhold = -1;
				break;
			case 'c':
				count_bytes = 1;
				/* fall through */
#endif
			case 'n':
				p = optarg;
			GET_COUNT:
				count = bb_xgetularg10(p);
				break;
			default:
				bb_show_usage();
		}
	}

	argv += optind;
	if (!*argv) {
		*--argv = "-";
	}

	fmt = header_fmt_str + 1;
#ifdef CONFIG_FEATURE_FANCY_HEAD
	if (argc - optind <= header_threshhold) {
		header_threshhold = 0;
	}
#else
	if (argc <= optind + 1) {
		fmt += 11;
	}
	/* Now define some things here to avoid #ifdefs in the code below.
	 * These should optimize out of the if conditions below. */
#define header_threshhold   1
#define count_bytes         0
#endif

	do {
		if ((fp = bb_wfopen_input(*argv)) != NULL) {
			if (fp == stdin) {
				*argv = (char *) bb_msg_standard_input;
			}
			if (header_threshhold) {
				bb_printf(fmt, *argv);
			}
			i = count;
			while (i && ((c = getc(fp)) != EOF)) {
				if (count_bytes || (c == '\n')) {
					--i;
				}
				putchar(c);
			}
			if (bb_fclose_nonstdin(fp)) {
				bb_perror_msg("%s", *argv);	/* Avoid multibyte problems. */
				retval = EXIT_FAILURE;
			}
			bb_xferror_stdout();
		}
		fmt = header_fmt_str;
	} while (*++argv);

	bb_fflush_stdout_and_exit(retval);
}
