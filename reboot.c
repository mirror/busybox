#include "internal.h"
#include <signal.h>

const char	reboot_usage[] = "reboot\n"
"\n\t"
"\treboot the system.\n";

extern int
reboot_main(struct FileInfo * i, int argc, char * * argv)
{
	return kill(1, SIGUSR2);
}
