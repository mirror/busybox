#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

extern int
length_main(int argc, char * * argv)
{
    if ( argc != 2 || **(argv+1) == '-' ) {
	usage("length string\n");
    }
    printf("%d\n", strlen(argv[1]));
    return( TRUE);
}
