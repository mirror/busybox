#include "internal.h"
#include <signal.h>

extern int
reboot_main(int argc, char ** argv)
{
	exit( kill(1, SIGUSR2));
}
