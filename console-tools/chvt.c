/* vi: set sw=4 ts=4: */
/*
 * chvt.c - aeb - 940227 - Change virtual terminal
 *
 * busyboxed by Erik Andersen
 */
#include "internal.h"
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/vt.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>

int chvt_main(int argc, char **argv)
{
	int fd, num;

	if ((argc != 2) || (**(argv + 1) == '-')) {
		usage ("chvt N\n"
#ifndef BB_FEATURE_TRIVIAL_HELP
				"\nChanges the foreground virtual terminal to /dev/ttyN\n"
#endif
				);
	}
	fd = get_console_fd("/dev/console");
	num = atoi(argv[1]);
	if (ioctl(fd, VT_ACTIVATE, num)) {
		perror("VT_ACTIVATE");
		exit(FALSE);
	}
	if (ioctl(fd, VT_WAITACTIVE, num)) {
		perror("VT_WAITACTIVE");
		exit(FALSE);
	}
	exit(TRUE);
}


/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
