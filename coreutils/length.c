/* vi: set sw=4 ts=4: */
#include "busybox.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern int length_main(int argc, char **argv)
{
	if (argc != 2 || **(argv + 1) == '-')
		usage(length_usage);
	printf("%lu\n", (long)strlen(argv[1]));
	return (TRUE);
}
