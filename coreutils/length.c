/* vi: set sw=4 ts=4: */

/* BB_AUDIT SUSv3 N/A -- Apparently a busybox (obsolete?) extension. */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "busybox.h"

int length_main(int argc, char **argv);
int length_main(int argc, char **argv)
{
	if ((argc != 2) ||  (**(++argv) == '-')) {
	    bb_show_usage();
	}

	printf("%lu\n", (unsigned long)strlen(*argv));

	fflush_stdout_and_exit(EXIT_SUCCESS);
}
