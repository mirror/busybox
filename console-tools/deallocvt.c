/* vi: set sw=4 ts=4: */
/*
 * disalloc.c - aeb - 940501 - Disallocate virtual terminal(s)
 * Renamed deallocvt.
 */
#include "internal.h"
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <stdio.h>

char *progname;

int deallocvt_main(int argc, char *argv[])
{
	int fd, num, i;

	if ((argc != 2) || (**(argv + 1) == '-')) {
		usage
			("deallocvt N\n\nDeallocate unused virtual terminal /dev/ttyN\n");
	}

	progname = argv[0];

	fd = get_console_fd("/dev/console");

	if (argc == 1) {
		/* deallocate all unused consoles */
		if (ioctl(fd, VT_DISALLOCATE, 0)) {
			perror("VT_DISALLOCATE");
			exit(1);
		}
	} else
		for (i = 1; i < argc; i++) {
			num = atoi(argv[i]);
			if (num == 0)
				fprintf(stderr, "%s: 0: illegal VT number\n", progname);
			else if (num == 1)
				fprintf(stderr, "%s: VT 1 cannot be deallocated\n",
						progname);
			else if (ioctl(fd, VT_DISALLOCATE, num)) {
				perror("VT_DISALLOCATE");
				fprintf(stderr, "%s: could not deallocate console %d\n",
						progname, num);
				exit(1);
			}
		}
	exit(0);
}
