/* vi: set sw=4 ts=4: */
/*
 * wc implementation for busybox
 *
 * Copyright (C) 2003  Manuel Novoa III  <mjn3@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* BB_AUDIT SUSv3 _NOT_ compliant -- option -m is not currently supported. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/wc.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Rewritten to fix a number of problems and do some size optimizations.
 * Problems in the previous busybox implementation (besides bloat) included:
 *  1) broken 'wc -c' optimization (read note below)
 *  2) broken handling of '-' args
 *  3) no checking of ferror on EOF returns
 *  4) isprint() wasn't considered when word counting.
 *
 * TODO:
 *
 * When locale support is enabled, count multibyte chars in the '-m' case.
 *
 * NOTES:
 *
 * The previous busybox wc attempted an optimization using stat for the
 * case of counting chars only.  I omitted that because it was broken.
 * It didn't take into account the possibility of input coming from a
 * pipe, or input from a file with file pointer not at the beginning.
 *
 * To implement such a speed optimization correctly, not only do you
 * need the size, but also the file position.  Note also that the
 * file position may be past the end of file.  Consider the example
 * (adapted from example in gnu wc.c)
 *
 *      echo hello > /tmp/testfile &&
 *      (dd ibs=1k skip=1 count=0 &> /dev/null; wc -c) < /tmp/testfile
 *
 * for which 'wc -c' should output '0'.
 */

#include "libbb.h"

#if ENABLE_LOCALE_SUPPORT
#define isspace_given_isprint(c) isspace(c)
#else
#undef isspace
#undef isprint
#define isspace(c) ((((c) == ' ') || (((unsigned int)((c) - 9)) <= (13 - 9))))
#define isprint(c) (((unsigned int)((c) - 0x20)) <= (0x7e - 0x20))
#define isspace_given_isprint(c) ((c) == ' ')
#endif

#if ENABLE_FEATURE_WC_LARGE
#define COUNT_T unsigned long long
#define COUNT_FMT "llu"
#else
#define COUNT_T unsigned
#define COUNT_FMT "u"
#endif

enum {
	WC_LINES	= 0,
	WC_WORDS	= 1,
	WC_CHARS	= 2,
	WC_LENGTH	= 3
};

int wc_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int wc_main(int argc UNUSED_PARAM, char **argv)
{
	FILE *fp;
	const char *s, *arg;
	const char *start_fmt = " %9"COUNT_FMT + 1;
	const char *fname_fmt = " %s\n";
	COUNT_T *pcounts;
	COUNT_T counts[4];
	COUNT_T totals[4];
	unsigned linepos;
	unsigned u;
	int num_files = 0;
	int c;
	smallint status = EXIT_SUCCESS;
	smallint in_word;
	unsigned print_type;

	print_type = getopt32(argv, "lwcL");

	if (print_type == 0) {
		print_type = (1 << WC_LINES) | (1 << WC_WORDS) | (1 << WC_CHARS);
	}

	argv += optind;
	if (!argv[0]) {
		*--argv = (char *) bb_msg_standard_input;
		fname_fmt = "\n";
		if (!((print_type-1) & print_type)) /* exactly one option? */
			start_fmt = "%"COUNT_FMT;
	}

	memset(totals, 0, sizeof(totals));

	pcounts = counts;

	while ((arg = *argv++) != 0) {
		++num_files;
		fp = fopen_or_warn_stdin(arg);
		if (!fp) {
			status = EXIT_FAILURE;
			continue;
		}

		memset(counts, 0, sizeof(counts));
		linepos = 0;
		in_word = 0;

		do {
			/* Our -w doesn't match GNU wc exactly... oh well */

			++counts[WC_CHARS];
			c = getc(fp);
			if (isprint(c)) {
				++linepos;
				if (!isspace_given_isprint(c)) {
					in_word = 1;
					continue;
				}
			} else if (((unsigned int)(c - 9)) <= 4) {
				/* \t  9
				 * \n 10
				 * \v 11
				 * \f 12
				 * \r 13
				 */
				if (c == '\t') {
					linepos = (linepos | 7) + 1;
				} else {			/* '\n', '\r', '\f', or '\v' */
				DO_EOF:
					if (linepos > counts[WC_LENGTH]) {
						counts[WC_LENGTH] = linepos;
					}
					if (c == '\n') {
						++counts[WC_LINES];
					}
					if (c != '\v') {
						linepos = 0;
					}
				}
			} else if (c == EOF) {
				if (ferror(fp)) {
					bb_simple_perror_msg(arg);
					status = EXIT_FAILURE;
				}
				--counts[WC_CHARS];
				goto DO_EOF;		/* Treat an EOF as '\r'. */
			} else {
				continue;
			}

			counts[WC_WORDS] += in_word;
			in_word = 0;
			if (c == EOF) {
				break;
			}
		} while (1);

		if (totals[WC_LENGTH] < counts[WC_LENGTH]) {
			totals[WC_LENGTH] = counts[WC_LENGTH];
		}
		totals[WC_LENGTH] -= counts[WC_LENGTH];

		fclose_if_not_stdin(fp);

	OUTPUT:
		/* coreutils wc tries hard to print pretty columns
		 * (saves results for all files, find max col len etc...)
		 * we won't try that hard, it will bloat us too much */
		s = start_fmt;
		u = 0;
		do {
			if (print_type & (1 << u)) {
				printf(s, pcounts[u]);
				s = " %9"COUNT_FMT; /* Ok... restore the leading space. */
			}
			totals[u] += pcounts[u];
		} while (++u < 4);
		printf(fname_fmt, arg);
	}

	/* If more than one file was processed, we want the totals.  To save some
	 * space, we set the pcounts ptr to the totals array.  This has the side
	 * effect of trashing the totals array after outputting it, but that's
	 * irrelavent since we no longer need it. */
	if (num_files > 1) {
		num_files = 0;				/* Make sure we don't get here again. */
		arg = "total";
		pcounts = totals;
		--argv;
		goto OUTPUT;
	}

	fflush_stdout_and_exit(status);
}
