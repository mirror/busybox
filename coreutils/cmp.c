/* vi: set sw=4 ts=4: */
/*
 * Mini cmp implementation for busybox
 *
 * Copyright (C) 2000,2001 by Matt Kraai <kraai@alumni.carnegiemellon.edu>
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

/* BB_AUDIT SUSv3 (virtually) compliant -- uses nicer GNU format for -l. */
/* http://www.opengroup.org/onlinepubs/007904975/utilities/cmp.html */

/* Mar 16, 2003      Manuel Novoa III   (mjn3@codepoet.org)
 *
 * Original version majorly reworked for SUSv3 compliance, bug fixes, and
 * size optimizations.  Changes include:
 * 1) Now correctly distingusishes between errors and actual file differences.
 * 2) Proper handling of '-' args.
 * 3) Actual error checking of i/o.
 * 4) Accept SUSv3 -l option.  Note that we use the slightly nicer gnu format
 *    in the '-l' case.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "busybox.h"

static FILE *cmp_xfopen_input(const char *filename)
{
	FILE *fp;

	if ((fp = bb_wfopen_input(filename)) != NULL) {
		return fp;
	}

	exit(bb_default_error_retval);	/* We already output an error message. */
}

static const char fmt_eof[] = "cmp: EOF on %s\n";
static const char fmt_differ[] = "%s %s differ: char %d, line %d\n";
#if 0
static const char fmt_l_opt[] = "%.0s%.0s%d %o %o\n";	/* SUSv3 format */
#else
static const char fmt_l_opt[] = "%.0s%.0s%d %3o %3o\n";	/* nicer gnu format */
#endif

static const char opt_chars[] = "sl";

enum {
	OPT_s = 1,
	OPT_l = 2
};

int cmp_main(int argc, char **argv)
{
	FILE *fp1, *fp2, *outfile = stdout;
	const char *filename1, *filename2;
	const char *fmt;
	int c1, c2, char_pos, line_pos;
	int opt_flags;
	int exit_val = 0;

	bb_default_error_retval = 2;	/* 1 is returned if files are different. */

	opt_flags = bb_getopt_ulflags(argc, argv, opt_chars);

	if ((opt_flags == 3) || (((unsigned int)(--argc - optind)) > 1)) {
		bb_show_usage();
	}

	fp1 = cmp_xfopen_input(filename1 = *(argv += optind));

	filename2 = "-";
	if (*++argv) {
		filename2 = *argv;
	}
	fp2 = cmp_xfopen_input(filename2);

	if (fp1 == fp2) {			/* Paranioa check... stdin == stdin? */
		/* Note that we don't bother reading stdin.  Neither does gnu wc.
		 * But perhaps we should, so that other apps down the chain don't
		 * get the input.  Consider 'echo hello | (cmp - - && cat -)'.
		 */
		return 0;
	}

	fmt = fmt_differ;
	if (opt_flags == OPT_l) {
		fmt = fmt_l_opt;
	}

	char_pos = 0;
	line_pos = 1;
	do {
		c1 = getc(fp1);
		c2 = getc(fp2);
		++char_pos;
		if (c1 != c2) {			/* Remember -- a read error may have occurred. */
			exit_val = 1;		/* But assume the files are different for now. */
			if (c2 == EOF) {
				/* We know that fp1 isn't at EOF or in an error state.  But to
				 * save space below, things are setup to expect an EOF in fp1
				 * if an EOF occurred.  So, swap things around.
				 */
				fp1 = fp2;
				filename1 = filename2;
				c1 = c2;
			}
			if (c1 == EOF) {
				bb_xferror(fp1, filename1);
				fmt = fmt_eof;	/* Well, no error, so it must really be EOF. */
				outfile = stderr;
				/* There may have been output to stdout (option -l), so
				 * make sure we fflush before writing to stderr. */
				bb_xfflush_stdout();
			}
			if (opt_flags != OPT_s) {
				if (opt_flags == OPT_l) {
					line_pos = c1;	/* line_pos is unused in the -l case. */
				}
				bb_fprintf(outfile, fmt, filename1, filename2, char_pos, line_pos, c2);
				if (opt_flags) {	/* This must be -l since not -s. */
					/* If we encountered and EOF, the while check will catch it. */
					continue;
				}
			}
			break;
		}
		if (c1 == '\n') {
			++line_pos;
		}
	} while (c1 != EOF);

	bb_xferror(fp1, filename1);
	bb_xferror(fp2, filename2);

	bb_fflush_stdout_and_exit(exit_val);
}
