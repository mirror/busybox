/* vi: set sw=4 ts=4: */
/*
 * disalloc.c - aeb - 940501 - Disallocate virtual terminal(s)
 * Renamed deallocvt.
 */
#include "busybox.h"
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>

/* From <linux/vt.h> */
#define VT_DISALLOCATE  0x5608  /* free memory associated to vt */

int deallocvt_main(int argc, char *argv[])
{
	int fd, num, i;

	//if ((argc > 2) || ((argv == 2) && (**(argv + 1) == '-')))
	if (argc > 2)
		usage(deallocvt_usage);

	fd = get_console_fd("/dev/console");

	if (argc == 1) {
printf("erik: A\n");
		/* deallocate all unused consoles */
		if (ioctl(fd, VT_DISALLOCATE, 0)) {
			perror("VT_DISALLOCATE");
			exit( FALSE);
		}
	} else
printf("erik: B\n");
		for (i = 1; i < argc; i++) {
			num = atoi(argv[i]);
			if (num == 0)
				errorMsg("0: illegal VT number\n");
			else if (num == 1)
				errorMsg("VT 1 cannot be deallocated\n");
			else if (ioctl(fd, VT_DISALLOCATE, num)) {
				perror("VT_DISALLOCATE");
				fatalError("could not deallocate console %d\n", num);
			}
		}
printf("erik: C\n");
	return( TRUE);
}
