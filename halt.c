#include "internal.h"
#include <signal.h>

extern int
halt_main(int argc, char ** argv)
{
    exit( kill(1, SIGUSR1));
}

