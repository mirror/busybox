/* vi: set sw=4 ts=4: */
/*
 * disalloc.c - aeb - 940501 - Disallocate virtual terminal(s)
 * Renamed deallocvt.
 */
#include "internal.h"
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

	if ((argc != 2) || (**(argv + 1) == '-')) {
		usage
			("deallocvt N\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
			 "\nDeallocate unused virtual terminal /dev/ttyN\n"
#endif
			 );
	}

	fd = get_console_fd("/dev/console");

	if (argc == 1) {
		/* deallocate all unused consoles */
		if (ioctl(fd, VT_DISALLOCATE, 0)) {
			perror("VT_DISALLOCATE");
			exit( FALSE);
		}
	} else
		for (i = 1; i < argc; i++) {
			num = atoi(argv[i]);
			if (num == 0)
				fprintf(stderr, "%s: 0: illegal VT number\n", applet_name);
			else if (num == 1)
				fprintf(stderr, "%s: VT 1 cannot be deallocated\n",
						applet_name);
			else if (ioctl(fd, VT_DISALLOCATE, num)) {
				perror("VT_DISALLOCATE");
				fprintf(stderr, "%s: could not deallocate console %d\n",
						applet_name, num);
				exit( FALSE);
			}
		}
	return( TRUE);
}
