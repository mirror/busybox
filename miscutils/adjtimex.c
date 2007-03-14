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

int adjtimex_main(int argc, char **argv);
int adjtimex_main(int argc, char **argv)
{
	enum {
		OPT_quiet = 0x1
	};
	unsigned opt;
	char *opt_o, *opt_f, *opt_p, *opt_t;
	struct timex txc;
	int i, ret, sep;
	const char *descript;
	txc.modes=0;

	opt = getopt32(argc, argv, "qo:f:p:t:",
			&opt_o, &opt_f, &opt_p, &opt_t);
	//if (opt & 0x1) // -q
	if (opt & 0x2) { // -o
		txc.offset = xatol(opt_o);
		txc.modes |= ADJ_OFFSET_SINGLESHOT;
	}
	if (opt & 0x4) { // -f
		txc.freq = xatol(opt_f);
		txc.modes |= ADJ_FREQUENCY;
	}
	if (opt & 0x8) { // -p
		txc.constant = xatol(opt_p);
		txc.modes |= ADJ_TIMECONST;
	}
	if (opt & 0x10) { // -t
		txc.tick = xatol(opt_t);
		txc.modes |= ADJ_TICK;
	}
	if (argc != optind) { /* no valid non-option parameters */
		bb_show_usage();
	}

	ret = adjtimex(&txc);

	if (ret < 0) perror("adjtimex");

	if (!(opt & OPT_quiet) && ret>=0) {
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
