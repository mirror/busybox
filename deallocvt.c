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
	int fd, num, i;

	//if ((argc > 2) || ((argv == 2) && (**(argv + 1) == '-')))
	if (argc > 2)
		show_usage();

	fd = get_console_fd();

	if (argc == 1) {
		/* deallocate all unused consoles */
		if (ioctl(fd, VT_DISALLOCATE, 0))
			perror_msg_and_die("VT_DISALLOCATE");
	} else {
		for (i = 1; i < argc; i++) {
			num = atoi(argv[i]);
			if (num == 0)
				error_msg("0: illegal VT number");
			else if (num == 1)
				error_msg("VT 1 cannot be deallocated");
			else if (ioctl(fd, VT_DISALLOCATE, num))
				perror_msg_and_die("VT_DISALLOCATE");
		}
	}

	return EXIT_SUCCESS;
}
