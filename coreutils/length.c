/* vi: set sw=4 ts=4: */
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern int length_main(int argc, char **argv)
{
	if (argc != 2 || **(argv + 1) == '-') {
		usage("length STRING\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
				"\nPrints out the length of the specified STRING.\n"
#endif
				);
	}
	printf("%lu\n", (long)strlen(argv[1]));
	return (TRUE);
}
