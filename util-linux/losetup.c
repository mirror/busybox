/* vi: set sw=4 ts=4: */
/*
 * Mini losetup implementation for busybox
 *
 * Copyright (C) 2002  Matt Kraai.
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <getopt.h>
#include <stdlib.h>

#include "busybox.h"

int losetup_main(int argc, char **argv)
{
	unsigned long opt;
	char *opt_o;
	int offset = 0;

	opt = bb_getopt_ulflags(argc, argv, "do:", &opt_o);
	argc -= optind;
	argv += optind;

	if (opt == 0x3) bb_show_usage(); // -d and -o (illegal)

	if (opt == 0x1) { // -d
		/* detach takes exactly one argument */
		if (argc != 1)
			bb_show_usage();
		if (!del_loop(argv[0]))
			return EXIT_SUCCESS;
		bb_perror_nomsg_and_die();
	}

	if (opt == 0x2) // -o
		offset = bb_xparse_number(opt_o, NULL);

	/* -o or no option */

	if (argc == 2) {
		if (set_loop(&argv[0], argv[1], offset) < 0)
			bb_perror_nomsg_and_die();
	} else if (argc == 1) {
		char *s = query_loop(argv[0]);
		if (!s) bb_perror_nomsg_and_die();
		printf("%s: %s\n", argv[0], s);
		if (ENABLE_FEATURE_CLEAN_UP) free(s);
	} else
		bb_show_usage();
	return EXIT_SUCCESS;
}
