/*
 * adjtimex.c - read, and possibly modify, the Linux kernel `timex' variables.
 *
 * Originally written: October 1997
 * Last hack: March 2001
 * Copyright 1997, 2000, 2001 Larry Doolittle <LRDoolittle@lbl.gov>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License (Version 2,
 *  June 1991) as published by the Free Software Foundation.  At the
 *  time of writing, that license was published by the FSF with the URL
 *  http://www.gnu.org/copyleft/gpl.html, and is incorporated herein by
 *  reference.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 * This adjtimex(1) is very similar in intent to adjtimex(8) by Steven
 * Dick <ssd@nevets.oau.org> and Jim Van Zandt <jrv@vanzandt.mv.com>
 * (see http://metalab.unc.edu/pub/Linux/system/admin/time/adjtimex*).
 * That version predates this one, and is _much_ bigger and more
 * featureful.  My independently written version was very similar to
 * Steven's from the start, because they both follow the kernel timex
 * structure.  I further tweaked this version to be equivalent to Steven's
 * where possible, but I don't like getopt_long, so the actual usage
 * syntax is incompatible.
 *
 * Amazingly enough, my Red Hat 5.2 sys/timex (and sub-includes)
 * don't actually give a prototype for adjtimex(2), so building
 * this code (with -Wall) gives a warning.  Later versions of
 * glibc fix this issue.
 *
 * This program is too simple for a Makefile, just build with:
 *  gcc -Wall -O adjtimex.c -o adjtimex
 *
 * busyboxed 20 March 2001, Larry Doolittle <ldoolitt@recycle.lbl.gov>
 * It will autosense if it is built in a busybox environment, based
 * on the BB_VER preprocessor macro.
 */

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/timex.h>
#include "busybox.h"

static struct {int bit; char *name;} statlist[] = {
	{ STA_PLL,       "PLL"       },
	{ STA_PPSFREQ,   "PPSFREQ"   },
	{ STA_PPSTIME,   "PPSTIME"   },
	{ STA_FLL,       "FFL"       },
	{ STA_INS,       "INS"       },
	{ STA_DEL,       "DEL"       },
	{ STA_UNSYNC,    "UNSYNC"    },
	{ STA_FREQHOLD,  "FREQHOLD"  },
	{ STA_PPSSIGNAL, "PPSSIGNAL" },
	{ STA_PPSJITTER, "PPSJITTER" },
	{ STA_PPSWANDER, "PPSWANDER" },
	{ STA_PPSERROR,  "PPSERROR"  },
	{ STA_CLOCKERR,  "CLOCKERR"  },
	{ 0, NULL } };

static char *ret_code_descript[] = {
	"clock synchronized",
	"insert leap second",
	"delete leap second",
	"leap second in progress",
	"leap second has occurred",
	"clock not synchronized" };

#ifdef BB_VER
#define main adjtimex_main
#else
void usage(char *prog)
{
	fprintf(stderr,
		"Usage: %s [ -q ] [ -o offset ] [ -f frequency ] [ -p timeconstant ] [ -t tick ]\n",
		prog);
}
#define bb_show_usage() usage(argv[0])
#endif

int main(int argc, char ** argv)
{
	struct timex txc;
	int quiet=0;
	int c, i, ret, sep;
	char *descript;
	txc.modes=0;
	for (;;) {
		c = getopt( argc, argv, "qo:f:p:t:");
		if (c == EOF) break;
		switch (c) {
			case 'q':
				quiet=1;
				break;
			case 'o':
				txc.offset = atoi(optarg);
				txc.modes |= ADJ_OFFSET_SINGLESHOT;
				break;
			case 'f':
				txc.freq = atoi(optarg);
				txc.modes |= ADJ_FREQUENCY;
				break;
			case 'p':
				txc.constant = atoi(optarg);
				txc.modes |= ADJ_TIMECONST;
				break;
			case 't':
				txc.tick = atoi(optarg);
				txc.modes |= ADJ_TICK;
				break;
			default:
				bb_show_usage();
				exit(1);
		}
	}
	if (argc != optind) { /* no valid non-option parameters */
		bb_show_usage();
		exit(1);
	}

	ret = adjtimex(&txc);

	if (ret < 0) perror("adjtimex");

	if (!quiet && ret>=0) {
		printf(
			"    mode:         %d\n"
			"-o  offset:       %ld\n"
			"-f  frequency:    %ld\n"
			"    maxerror:     %ld\n"
			"    esterror:     %ld\n"
			"    status:       %d ( ",
		txc.modes, txc.offset, txc.freq, txc.maxerror,
		txc.esterror, txc.status);

		/* representative output of next code fragment:
		   "PLL | PPSTIME" */
		sep=0;
		for (i=0; statlist[i].name; i++) {
			if (txc.status & statlist[i].bit) {
				if (sep) fputs(" | ",stdout);
				fputs(statlist[i].name,stdout);
				sep=1;
			}
		}

		descript = "error";
		if (ret >= 0 && ret <= 5) descript = ret_code_descript[ret];
		printf(" )\n"
			"-p  timeconstant: %ld\n"
			"    precision:    %ld\n"
			"    tolerance:    %ld\n"
			"-t  tick:         %ld\n"
			"    time.tv_sec:  %ld\n"
			"    time.tv_usec: %ld\n"
			"    return value: %d (%s)\n",
		txc.constant,
		txc.precision, txc.tolerance, txc.tick,
		(long)txc.time.tv_sec, (long)txc.time.tv_usec, ret, descript);
	}
	return (ret<0);
}
