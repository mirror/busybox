/* vi: set sw=4 ts=4: */
/*
 * Mini fsync implementation for busybox
 *
 * Copyright (C) 2008 Nokia Corporation. All rights reserved.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */
//config:config FSYNC
//config:	bool "fsync (3.6 kb)"
//config:	default y
//config:	help
//config:	fsync is used to flush file-related cached blocks to disk.

//                APPLET_NOFORK:name   main   location    suid_type     help
//applet:IF_FSYNC(APPLET_NOFORK(fsync, fsync, BB_DIR_BIN, BB_SUID_DROP, fsync))

//kbuild:lib-$(CONFIG_FSYNC) += fsync.o

//usage:#define fsync_trivial_usage
//usage:       "[-d] FILE..."
//usage:#define fsync_full_usage "\n\n"
//usage:       "Write all buffered blocks in FILEs to disk\n"
//usage:     "\n	-d	Avoid syncing metadata"

#include "libbb.h"

/* This is a NOFORK applet. Be very careful! */

int fsync_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int fsync_main(int argc UNUSED_PARAM, char **argv)
{
	int ret;
	int opts;

	opts = getopt32(argv, "d"); /* fdatasync */
	argv += optind;
	if (!*argv) {
		bb_show_usage();
	}

	ret = EXIT_SUCCESS;
	do {
		/* GNU "sync FILE" uses O_NONBLOCK open */
		int fd = open_or_warn(*argv, /*O_NOATIME |*/ O_NOCTTY | O_RDONLY | O_NONBLOCK);
		/* open(NOATIME) can only be used by owner or root, don't use NOATIME here */

		if (fd < 0) {
			ret = EXIT_FAILURE;
			goto next;
		}
		if ((opts ? fdatasync(fd) : fsync(fd)) != 0) {
			bb_simple_perror_msg(*argv);
			ret = EXIT_FAILURE;
		}
		close(fd);
 next:
		argv++;
	} while (*argv);

	return ret;
}
