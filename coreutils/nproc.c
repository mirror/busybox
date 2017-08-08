/*
 * Copyright (C) 2017 Denys Vlasenko <vda.linux@googlemail.com>
 *
 * Licensed under GPLv2, see LICENSE in this source tree
 */
//config:config NPROC
//config:	bool "nproc (248 bytes)"
//config:	default y
//config:	help
//config:	Print number of CPUs

//applet:IF_NPROC(APPLET_NOFORK(nproc, nproc, BB_DIR_USR_BIN, BB_SUID_DROP, nproc))

//kbuild:lib-$(CONFIG_NPROC) += nproc.o

//usage:#define nproc_trivial_usage
//usage:	""
//TODO: "[--all] [--ignore=N]"
//usage:#define nproc_full_usage "\n\n"
//usage:	"Print number of CPUs"

#include <sched.h>
#include "libbb.h"

int nproc_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int nproc_main(int argc UNUSED_PARAM, char **argv UNUSED_PARAM)
{
	unsigned long mask[1024];
	unsigned i, count = 0;

	//getopt32(argv, "");

	//if --all, count /sys/devices/system/cpu/cpuN dirs, else:

	if (sched_getaffinity(0, sizeof(mask), (void*)mask) == 0) {
		for (i = 0; i < ARRAY_SIZE(mask); i++) {
			unsigned long m = mask[i];
			while (m) {
				if (m & 1)
					count++;
				m >>= 1;
			}
		}
	}
	if (count == 0)
		count++;
	printf("%u\n", count);

	return 0;
}
