/* vi: set sw=4 ts=4: */
/*
 * adjtimex.c - read, and possibly modify, the Linux kernel `timex' variables.
 *
 * Originally written: October 1997
 * Last hack: March 2001
 * Copyright 1997, 2000, 2001 Larry Doolittle <LRDoolittle@lbl.gov>
 *
 * busyboxed 20 March 2001, Larry Doolittle <ldoolitt@recycle.lbl.gov>
 *
 * Licensed under GPLv2 or later, see file License in this tarball for details.
 */

#include "busybox.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/timex.h>

static const struct {int bit; const char *name;} statlist[] = {
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

static const char * const ret_code_descript[] = {
	"clock synchronized",
	"insert leap second",
	"delete leap second",
	"leap second in progress",
	"leap second has occurred",
	"clock not synchronized" };

int adjtimex_main(int argc, char **argv)
{
	struct timex txc;
	int quiet=0;
	int c, i, ret, sep;
	const char *descript;
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
