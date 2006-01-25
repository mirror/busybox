/*
 *  readprofile.c - used to read /proc/profile
 *
 *  Copyright (C) 1994,1996 Alessandro Rubini (rubini@ipvvis.unipv.it)
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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

#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>

#include "busybox.h"

#define S_LEN 128

/* These are the defaults */
static const char defaultmap[]="/boot/System.map";
static const char defaultpro[]="/proc/profile";

int readprofile_main(int argc, char **argv)
{
	FILE *map;
	int proFd;
	const char *mapFile, *proFile, *mult=0;
	unsigned long len=0, indx=1;
	unsigned long long add0=0;
	unsigned int step;
	unsigned int *buf, total, fn_len;
	unsigned long long fn_add, next_add;          /* current and next address */
	char fn_name[S_LEN], next_name[S_LEN];   /* current and next name */
	char mode[8];
	int c;
	int optAll=0, optInfo=0, optReset=0, optVerbose=0, optNative=0;
	int optBins=0, optSub=0;
	char mapline[S_LEN];
	int maplineno=1;
	int header_printed;

#define next (current^1)

	proFile = defaultpro;
	mapFile = defaultmap;

	while ((c = getopt(argc, argv, "M:m:np:itvarVbs")) != -1) {
		switch(c) {
		case 'm':
			mapFile = optarg;
			break;
		case 'n':
			optNative++;
			break;
		case 'p':
			proFile = optarg;
			break;
		case 'a':
			optAll++;
			break;
		case 'b':
			optBins++;
			break;
		case 's':
			optSub++;
			break;
		case 'i':
			optInfo++;
			break;
		case 'M':
			mult = optarg;
			break;
		case 'r':
			optReset++;
			break;
		case 'v':
			optVerbose++;
			break;
		default:
			bb_show_usage();
		}
	}

	if (optReset || mult) {
		int multiplier, fd, to_write;

		/*
		 * When writing the multiplier, if the length of the write is
		 * not sizeof(int), the multiplier is not changed
		 */
		if (mult) {
			multiplier = strtoul(mult, 0, 10);
			to_write = sizeof(int);
		} else {
			multiplier = 0;
			to_write = 1;	/* sth different from sizeof(int) */
		}

		fd = bb_xopen(defaultpro,O_WRONLY);

		if (write(fd, &multiplier, to_write) != to_write)
			bb_perror_msg_and_die("error writing %s", defaultpro);

		close(fd);
		return EXIT_SUCCESS;
	}

	/*
	 * Use an fd for the profiling buffer, to skip stdio overhead
	 */

	proFd = bb_xopen(proFile,O_RDONLY);

	if (((int)(len=lseek(proFd,0,SEEK_END)) < 0)
	    || (lseek(proFd,0,SEEK_SET) < 0))
		bb_perror_msg_and_die(proFile);

	buf = xmalloc(len);

	if (read(proFd,buf,len) != len)
		bb_perror_msg_and_die(proFile);

	close(proFd);

	if (!optNative) {
		int entries = len/sizeof(*buf);
		int big = 0,small = 0,i;
		unsigned *p;

		for (p = buf+1; p < buf+entries; p++) {
			if (*p & ~0U << (sizeof(*buf)*4))
				big++;
			if (*p & ((1 << (sizeof(*buf)*4))-1))
				small++;
		}
		if (big > small) {
			bb_error_msg("Assuming reversed byte order. "
				"Use -n to force native byte order.");
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

	map = bb_xfopen(mapFile, "r");

	while (fgets(mapline,S_LEN,map)) {
		if (sscanf(mapline,"%llx %s %s",&fn_add,mode,fn_name) != 3)
			bb_error_msg_and_die("%s(%i): wrong map line",
					     mapFile, maplineno);

		if (!strcmp(fn_name,"_stext")) /* only elf works like this */ {
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
	while (fgets(mapline,S_LEN,map)) {
		unsigned int this = 0;

		if (sscanf(mapline,"%llx %s %s",&next_add,mode,next_name) != 3)
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
					printf ("%s:\n", fn_name);
					header_printed = 1;
				}
				printf ("\t%llx\t%u\n", (indx - 1)*step + add0, buf[indx]);
			}
			this += buf[indx++];
		}
		total += this;

		if (optBins) {
			if (optVerbose || this > 0)
				printf ("  total\t\t\t\t%u\n", this);
		} else if ((this || optAll) &&
			   (fn_len = next_add-fn_add) != 0) {
			if (optVerbose)
				printf("%016llx %-40s %6i %8.4f\n", fn_add,
				       fn_name,this,this/(double)fn_len);
			else
				printf("%6i %-40s %8.4f\n",
				       this,fn_name,this/(double)fn_len);
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
		strcpy(fn_name,next_name);

		maplineno++;
	}

	/* clock ticks, out of kernel text - probably modules */
	printf("%6i %s\n", buf[len/sizeof(*buf)-1], "*unknown*");

	/* trailer */
	if (optVerbose)
		printf("%016x %-40s %6i %8.4f\n",
		       0,"total",total,total/(double)(fn_add-add0));
	else
		printf("%6i %-40s %8.4f\n",
		       total,"total",total/(double)(fn_add-add0));

	fclose(map);
	free(buf);

	return EXIT_SUCCESS;
}
