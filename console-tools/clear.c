#include "internal.h"
#include <stdio.h>

const char	clear_usage[] = "clear\n"
"\n"
"\tClears the screen.\n";

extern int
clear_main(struct FileInfo * i, int argc, char * * argv)
{
	printf("\033[H\033[J");
	return 0;
}
