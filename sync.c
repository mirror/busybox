#include "internal.h"
#include <stdio.h>

extern int
sync_main(int argc, char * * argv)
{
    if ( **(argv+1) == '-' ) {
	usage( "sync\nWrite all buffered filesystem blocks to disk.\n");
    }
    return sync();
}

