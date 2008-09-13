/* vi: set sw=4 ts=4: */
/*
 * Mini rmmod implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2008 Timo Teras <timo.teras@iki.fi>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include "modutils.h"

int rmmod_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int rmmod_main(int argc UNUSED_PARAM, char **argv)
{
	int n;
	unsigned int flags = O_NONBLOCK|O_EXCL;

	/* Parse command line. */
	n = getopt32(argv, "wfas"); // -s ignored
	argv += optind;

	if (n & 1)	// --wait
		flags &= ~O_NONBLOCK;
	if (n & 2)	// --force
		flags |= O_TRUNC;
	if (n & 4) {
		/* Unload _all_ unused modules via NULL delete_module() call */
		if (bb_delete_module(NULL, flags) != 0 && errno != EFAULT)
			bb_perror_msg_and_die("rmmod");
		return EXIT_SUCCESS;
	}

	if (!*argv)
		bb_show_usage();

	while (*argv) {
		char modname[MODULE_NAME_LEN];
		filename2modname(bb_basename(*argv++), modname);
		if (bb_delete_module(modname, flags))
			bb_error_msg_and_die("cannot unload '%s': %s",
					     modname, moderror(errno));
	}

	return EXIT_SUCCESS;
}
