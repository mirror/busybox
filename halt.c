#include "internal.h"
#include <signal.h>

const char	halt_usage[] = "halt\n"
"\n\t"
"\thalt the system.\n";

extern int
halt_main(struct FileInfo * i, int argc, char * * argv)
{
	return kill(1, SIGUSR1);
}
