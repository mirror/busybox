/* vi: set sw=4 ts=4: */
/*
 * pivot_root.c - Change root file system.  Based on util-linux 2.10s
 *
 * busyboxed by Evin Robertson
 */
#include "busybox.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <linux/unistd.h>

#ifndef __NR_pivot_root
#error Sorry, but this kernel does not support the pivot_root syscall
#endif

static _syscall2(int,pivot_root,const char *,new_root,const char *,put_old)


int pivot_root_main(int argc, char **argv)
{
    if (argc != 3)
        show_usage();

    if (pivot_root(argv[1],argv[2]) < 0)
        perror_msg_and_die("pivot_root");

    return EXIT_SUCCESS;

}


/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
