/* vi: set sw=4 ts=4: */
/*
 * seq implementation for busybox
 *
 * Copyright (C) 2004, Glenn McGrath
 *
 * Licensed under the GPL v2, see the file LICENSE in this tarball.
 */

#include <stdio.h>
#include <stdlib.h>
#include "busybox.h"

int seq_main(int argc, char **argv)
{
	double last, first, increment, i;

	first = increment = 1;
	switch (argc) {
		case 4:
			increment = atof(argv[2]);
		case 3:
			first = atof(argv[1]);
		case 2:
			last = atof(argv[argc-1]);
			break;
		default:
			bb_show_usage();
	}

	/* You should note that this is pos-5.0.91 semantics, -- FK. */
	for (i = first;
		(increment > 0 && i <= last) || (increment < 0 && i >=last);
		i += increment)
	{
		printf("%g\n", i);
	}

	return EXIT_SUCCESS;
}
