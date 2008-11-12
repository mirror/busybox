/* vi: set sw=4 ts=4: */
/*
 * seq implementation for busybox
 *
 * Copyright (C) 2004, Glenn McGrath
 *
 * Licensed under the GPL v2, see the file LICENSE in this tarball.
 */

#include "libbb.h"

/* This is a NOFORK applet. Be very careful! */


int seq_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int seq_main(int argc, char **argv)
{
	enum {
		OPT_w = (1 << 0),
		OPT_s = (1 << 1),
	};
	double last, increment, i;
	const char *sep, *opt_s = "\n";
	unsigned opt = getopt32(argv, "+ws:", &opt_s);
	unsigned width = 0;

	argc -= optind;
	argv += optind;
	i = increment = 1;
	switch (argc) {
		case 3:
			increment = atof(argv[1]);
		case 2:
			i = atof(*argv);
		case 1:
			last = atof(argv[argc-1]);
			break;
		default:
			bb_show_usage();
	}
	if (opt & OPT_w) /* Pad to length of start or last */
		width = MAX(strlen(*argv), strlen(argv[argc-1]));

	/* You should note that this is pos-5.0.91 semantics, -- FK. */
	sep = "";
	while ((increment > 0 && i <= last) || (increment < 0 && i >= last)) {
		printf("%s%0*g", sep, width, i);
		sep = opt_s;
		i += increment;
	}
	bb_putchar('\n');
	return fflush(stdout);
}
