#include "internal.h"

extern int
sync_main(int argc, char * * argv)
{
    if ( **(argv+1) == '-' ) {
	fprintf(stderr, "Usage: sync\nWrite all buffered filesystem blocks to disk.\n");
	exit(FALSE);
    }
	return sync();
}

