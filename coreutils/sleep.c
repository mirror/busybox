#include "internal.h"
#include <stdio.h>

const char	sleep_usage[] = "sleep seconds\n"
"\n"
"\tPause program execution for the given number of seconds.\n";

extern int
sleep_main(struct FileInfo * i, int argc, char * * argv)
{
	if ( sleep(atoi(argv[1])) != 0 )
		return -1;
	else
		return 0;
}
