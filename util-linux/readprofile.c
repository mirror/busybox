/* vi: set sw=4 ts=4: */
/*
 *  readprofile.c - used to read /proc/profile
 *
 *  Copyright (C) 1994,1996 Alessandro Rubini (rubini@ipvvis.unipv.it)
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/*
 * 1999-02-22 Arkadiusz Mi¶kiewicz <misiek@pld.ORG.PL>
 * - added Native Language Support
 * 1999-09-01 Stephane Eranian <eranian@cello.hpl.hp.com>
 * - 64bit clean patch
 * 3Feb2001 Andrew Morton <andrewm@uow.edu.au>
 * - -M option to write profile multiplier.
 * 2001-11-07 Werner Almesberger <wa@almesberger.net>
 * - byte order auto-detection and -n option
 * 2001-11-09 Werner Almesberger <wa@almesberger.net>
 * - skip step size (index 0)
 * 2002-03-09 John Levon <moz@compsoc.man.ac.uk>
 * - make maplineno do something
 * 2002-11-28 Mads Martin Joergensen +
 * - also try /boot/System.map-`uname -r`
 * 2003-04-09 Werner Almesberger <wa@almesberger.net>
 * - fixed off-by eight error and improved heuristics in byte order detection
 * 2003-08-12 Nikita Danilov <Nikita@Namesys.COM>
 * - added -s option; example of use:
 * "readprofile -s -m /boot/System.map-test | grep __d_lookup | sort -n -k3"
 *
 * Taken from util-linux and adapted for busybox by
 * Paul Mundt <lethal@linux-sh.org>.
 */

#include "libbb.h"
#include <sys/utsname.h>

#define S_LEN 128

/* These are the defaults */
static const char defaultmap[] ALIGN1 = "/boot/System.map";
static const char defaultpro[] ALIGN1 = "/proc/profile";

int readprofile_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int readprofile_main(int argc, char **argv)
{
	FILE *map;
	const char *mapFile, *proFile, *mult = 0;
	unsigned long indx = 1;
	size_t len;
	uint64_t add0 = 0;
	unsigned int step;
	unsigned int *buf, total, fn_len;
	unsigned long long fn_add, next_add;     /* current and next address */
	char fn_name[S_LEN], next_name[S_LEN];   /* current and next name */
	char mapline[S_LEN];
	char mode[8];
	int optAll = 0, optInfo = 0, optReset = 0;
	int optVerbose = 0, optNative = 0;
	int optBins = 0, optSub = 0;
	int maplineno = 1;
	int header_printed;

#define next (current^1)

	proFile = defaultpro;
	mapFile = defaultmap;

	opt_complementary = "nn:aa:bb:ss:ii:rr:vv";
	getopt32(argv, "M:m:p:nabsirv",
			&mult, &mapFile, &proFile,
			&optNative, &optAll, &optBins, &optSub,
			&optInfo, &optReset, &optVerbose);

	if (optReset || mult) {
		int multiplier, fd, to_write;

		/*
		 * When writing the multiplier, if the length of the write is
		 * not sizeof(int), the multiplier is not changed
		 */
		if (mult) {
			multiplier = xatoi_u(mult);
			to_write = sizeof(int);
		} else {
			multiplier = 0;
			to_write = 1;	/* sth different from sizeof(int) */
		}

		fd = xopen(defaultpro, O_WRONLY);
		xwrite(fd, &multiplier, to_write);
		close(fd);
		return EXIT_SUCCESS;
	}

	/*
	 * Use an fd for the profiling buffer, to skip stdio overhead
	 */
	len = MAXINT(ssize_t);
	buf = xmalloc_open_read_close(proFile, &len);
	if (!optNative) {
		int entries = len/sizeof(*buf);
		int big = 0, small = 0, i;
		unsigned *p;

		for (p = buf+1; p < buf+entries; p++) {
			if (*p & ~0U << (sizeof(*buf)*4))
				big++;
			if (*p & ((1 << (sizeof(*buf)*4))-1))
				small++;
		}
		if (big > small) {
			bb_error_msg("assuming reversed byte order, "
				"use -n to force native byte order");
			for (p = buf; p < buf+entries; p++)
				for (i = 0; i < sizeof(*buf)/2; i++) {
					unsigned char *b = (unsigned char *) p;
					unsigned char tmp;

					tmp = b[i];
					b[i] = b[sizeof(*buf)-i-1];
					b[sizeof(*buf)-i-1] = tmp;
				}
		}
	}

	step = buf[0];
	if (optInfo) {
		printf("Sampling_step: %i\n", step);
		return EXIT_SUCCESS;
	}

	total = 0;

	map = xfopen(mapFile, "r");

	while (fgets(mapline, S_LEN, map)) {
		if (sscanf(mapline, "%llx %s %s", &fn_add, mode, fn_name) != 3)
			bb_error_msg_and_die("%s(%i): wrong map line",
					     mapFile, maplineno);

		if (!strcmp(fn_name, "_stext")) /* only elf works like this */ {
			add0 = fn_add;
			break;
		}
		maplineno++;
	}

	if (!add0)
		bb_error_msg_and_die("can't find \"_stext\" in %s", mapFile);

	/*
	 * Main loop.
	 */
	while (fgets(mapline, S_LEN, map)) {
		unsigned int this = 0;

		if (sscanf(mapline, "%llx %s %s", &next_add, mode, next_name) != 3)
			bb_error_msg_and_die("%s(%i): wrong map line",
					mapFile, maplineno);

		header_printed = 0;

		/* ignore any LEADING (before a '[tT]' symbol is found)
		   Absolute symbols */
		if ((*mode == 'A' || *mode == '?') && total == 0) continue;
		if (*mode != 'T' && *mode != 't' &&
		    *mode != 'W' && *mode != 'w')
			break;	/* only text is profiled */

		if (indx >= len / sizeof(*buf))
			bb_error_msg_and_die("profile address out of range. "
					     "Wrong map file?");

		while (indx < (next_add-add0)/step) {
			if (optBins && (buf[indx] || optAll)) {
				if (!header_printed) {
					printf("%s:\n", fn_name);
					header_printed = 1;
				}
				printf("\t%"PRIx64"\t%u\n", (indx - 1)*step + add0, buf[indx]);
			}
			this += buf[indx++];
		}
		total += this;

		if (optBins) {
			if (optVerbose || this > 0)
				printf("  total\t\t\t\t%u\n", this);
		} else if ((this || optAll) &&
			   (fn_len = next_add-fn_add) != 0) {
			if (optVerbose)
				printf("%016llx %-40s %6i %8.4f\n", fn_add,
				       fn_name, this, this/(double)fn_len);
			else
				printf("%6i %-40s %8.4f\n",
				       this, fn_name, this/(double)fn_len);
			if (optSub) {
				unsigned long long scan;

				for (scan = (fn_add-add0)/step + 1;
				     scan < (next_add-add0)/step; scan++) {
					unsigned long long addr;

					addr = (scan - 1)*step + add0;
					printf("\t%#llx\t%s+%#llx\t%u\n",
					       addr, fn_name, addr - fn_add,
					       buf[scan]);
				}
			}
		}

		fn_add = next_add;
		strcpy(fn_name, next_name);

		maplineno++;
	}

	/* clock ticks, out of kernel text - probably modules */
	printf("%6i %s\n", buf[len/sizeof(*buf)-1], "*unknown*");

	/* trailer */
	if (optVerbose)
		printf("%016x %-40s %6i %8.4f\n",
		       0, "total", total, total/(double)(fn_add-add0));
	else
		printf("%6i %-40s %8.4f\n",
		       total, "total", total/(double)(fn_add-add0));

	fclose(map);
	free(buf);

	return EXIT_SUCCESS;
}
