#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

const char			length_usage[] = "length string";

int
length_main(struct FileInfo * i, int argc, char * * argv)
{
	printf("%d\n", strlen(argv[1]));
	return 0;
}
