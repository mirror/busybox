/* vi: set sw=4 ts=4: */
/*
 * Mini free implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2, see file LICENSE in this source tree.
 */

/* getopt not needed */

#include "libbb.h"

int free_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int free_main(int argc UNUSED_PARAM, char **argv IF_NOT_DESKTOP(UNUSED_PARAM))
{
	struct sysinfo info;
	unsigned mem_unit;

#if ENABLE_DESKTOP
	if (argv[1] && argv[1][0] == '-')
		bb_show_usage();
#endif

	sysinfo(&info);

	/* Kernels prior to 2.4.x will return info.mem_unit==0, so cope... */
	mem_unit = 1;
	if (info.mem_unit != 0) {
		mem_unit = info.mem_unit;
	}

	/* Convert values to kbytes */
	if (mem_unit == 1) {
		info.totalram >>= 10;
		info.freeram >>= 10;
#if BB_MMU
		info.totalswap >>= 10;
		info.freeswap >>= 10;
#endif
		info.sharedram >>= 10;
		info.bufferram >>= 10;
	} else {
		mem_unit >>= 10;
		/* TODO:  Make all this stuff not overflow when mem >= 4 Tb */
		info.totalram *= mem_unit;
		info.freeram *= mem_unit;
#if BB_MMU
		info.totalswap *= mem_unit;
		info.freeswap *= mem_unit;
#endif
		info.sharedram *= mem_unit;
		info.bufferram *= mem_unit;
	}

	printf("     %13s%13s%13s%13s%13s\n",
		"total",
		"used",
		"free",
		"shared", "buffers" /* swap and total don't have these columns */
		/* procps version 3.2.8 also shows "cached" column, but
		 * sysinfo() does not provide this value, need to parse
		 * /proc/meminfo instead and get "Cached: NNN kB" from there.
		 */
	);
#define FIELDS_5 "%13lu%13lu%13lu%13lu%13lu\n"
#define FIELDS_3 (FIELDS_5 + 2*5)
#define FIELDS_2 (FIELDS_5 + 3*5)
	printf("Mem: ");
	printf(FIELDS_5,
		info.totalram,
		info.totalram - info.freeram,
		info.freeram,
		info.sharedram, info.bufferram
	);
	/* Show alternate, more meaningful busy/free numbers by counting
	 * buffer cache as free memory (make it "-/+ buffers/cache"
	 * if/when we add support for "cached" column): */
	printf("-/+ buffers:      ");
	printf(FIELDS_2,
		info.totalram - info.freeram - info.bufferram,
		info.freeram + info.bufferram
	);
#if BB_MMU
	printf("Swap:");
	printf(FIELDS_3,
		info.totalswap,
		info.totalswap - info.freeswap,
		info.freeswap
	);
#endif
	return EXIT_SUCCESS;
}
