#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern int
length_main(int argc, char * * argv)
{
    if ( **(argv+1) == '-' ) {
	fprintf(stderr, "Usage: length string\n");
	exit(FALSE);
    }
    printf("%d\n", strlen(argv[1]));
    return( TRUE);
}
