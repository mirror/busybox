/* vi: set sw=4 ts=4: */
/*
 * disalloc.c - aeb - 940501 - Disallocate virtual terminal(s)
 * Renamed deallocvt.
 */
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include "busybox.h"

/* From <linux/vt.h> */
static const int VT_DISALLOCATE = 0x5608;  /* free memory associated to vt */

int deallocvt_main(int argc, char *argv[])
{
	int fd, num=0;

	if (argc > 2)
		bb_show_usage();

	fd = get_console_fd();
	
	/*  num=0  deallocate all unused consoles */
	if (argc == 1)
		goto disallocate_all;

	num=bb_xgetlarg(argv[1], 10, 0, INT_MAX);

	switch(num)
	{
		case 0:
			bb_error_msg("0: illegal VT number");
			break;
		case 1:
			bb_error_msg("VT 1 cannot be deallocated");
			break;
		default:
disallocate_all:
			if (ioctl(fd, VT_DISALLOCATE, num))
				bb_perror_msg_and_die("VT_DISALLOCATE");
			return EXIT_SUCCESS;
	}
	return EXIT_FAILURE;
}
