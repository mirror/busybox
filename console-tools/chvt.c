/* vi: set sw=4 ts=4: */
/*
 * chvt.c - aeb - 940227 - Change virtual terminal
 *
 * busyboxed by Erik Andersen
 */

/* getopt not needed */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include "busybox.h"

/* From <linux/vt.h> */
static const int VT_ACTIVATE = 0x5606;  /* make vt active */
static const int VT_WAITACTIVE = 0x5607;  /* wait for vt active */

int chvt_main(int argc, char **argv)
{
	int fd, num;

	if ((argc != 2) || (**(argv + 1) == '-'))
		show_usage();
	fd = get_console_fd();
	num = atoi(argv[1]);
	if (ioctl(fd, VT_ACTIVATE, num))
		perror_msg_and_die("VT_ACTIVATE");
	if (ioctl(fd, VT_WAITACTIVE, num))
		perror_msg_and_die("VT_WAITACTIVE");
	return EXIT_SUCCESS;
}


/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
