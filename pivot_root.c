/* vi: set sw=4 ts=4: */
/*
 * pivot_root.c - Change root file system.  Based on util-linux 2.10s
 *
 * busyboxed by Evin Robertson
 * pivot_root syscall stubbed by Erik Andersen, so it will compile
 *     regardless of the kernel being used. 
 */
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/syscall.h>
#include <linux/unistd.h>
#include "busybox.h"

#ifndef __NR_pivot_root
#warning This kernel does not support the pivot_root syscall
#warning The pivot_root system call is being stubbed out...
int pivot_root(const char * new_root,const char * put_old)
{
	/* BusyBox was compiled against a kernel that did not support
	 *  the pivot_root system call.  To make this application work,
	 *  you will need to recompile with a kernel supporting the
	 *  pivot_root system call.
	 */
	fprintf(stderr, "\n\nTo make this application work, you will need to recompile\n");
	fprintf(stderr, "with a kernel supporting the pivot_root system call. -Erik\n\n");
	errno=ENOSYS;
	return -1;
}
#else
static _syscall2(int,pivot_root,const char *,new_root,const char *,put_old)
#endif



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
