#include "internal.h"

const char	sync_usage[] = "sync\n"
"\n"
"\tWrite all buffered filesystem blocks to disk.\n";

extern int
sync_main(struct FileInfo * i, int argc, char * * argv)
{
	return sync();
}
