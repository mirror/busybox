/* vi: set sw=4 ts=4: */
/*
 * Mini free implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under the GPL version 2, see the file LICENSE in this tarball.
 */

/* getopt not needed */

#include "busybox.h"

int free_main(int argc, char **argv);
int free_main(int argc, char **argv)
{
	struct sysinfo info;
	sysinfo(&info);

	/* Kernels prior to 2.4.x will return info.mem_unit==0, so cope... */
	if (info.mem_unit==0) {
		info.mem_unit=1;
	}
	if ( info.mem_unit == 1 ) {
		info.mem_unit=1024;

		/* TODO:  Make all this stuff not overflow when mem >= 4 Gib */
		info.totalram/=info.mem_unit;
		info.freeram/=info.mem_unit;
#ifndef __uClinux__
		info.totalswap/=info.mem_unit;
		info.freeswap/=info.mem_unit;
#endif
		info.sharedram/=info.mem_unit;
		info.bufferram/=info.mem_unit;
	} else {
		info.mem_unit/=1024;
		/* TODO:  Make all this stuff not overflow when mem >= 4 Gib */
		info.totalram*=info.mem_unit;
		info.freeram*=info.mem_unit;
#ifndef __uClinux__
		info.totalswap*=info.mem_unit;
		info.freeswap*=info.mem_unit;
#endif
		info.sharedram*=info.mem_unit;
		info.bufferram*=info.mem_unit;
	}

	if (argc > 1 && **(argv + 1) == '-')
		bb_show_usage();

	printf("%6s%13s%13s%13s%13s%13s\n", "", "total", "used", "free",
			"shared", "buffers");

	printf("%6s%13ld%13ld%13ld%13ld%13ld\n", "Mem:", info.totalram,
			info.totalram-info.freeram, info.freeram,
			info.sharedram, info.bufferram);

#ifndef __uClinux__
	printf("%6s%13ld%13ld%13ld\n", "Swap:", info.totalswap,
			info.totalswap-info.freeswap, info.freeswap);

	printf("%6s%13ld%13ld%13ld\n", "Total:", info.totalram+info.totalswap,
			(info.totalram-info.freeram)+(info.totalswap-info.freeswap),
			info.freeram+info.freeswap);
#endif
	return EXIT_SUCCESS;
}

