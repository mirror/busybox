/* vi: set sw=4 ts=4: */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "busybox.h"

extern int length_main(int argc, char **argv)
{
	if (argc != 2 || **(argv + 1) == '-')
		show_usage();
	printf("%lu\n", (long)strlen(argv[1]));
	return EXIT_SUCCESS;
}
