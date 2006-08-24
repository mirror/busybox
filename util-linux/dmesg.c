/* vi: set ts=4:
 * 
 * dmesg - display/control kernel ring buffer.
 *
 * Copyring 2006 Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include <unistd.h>
#include <sys/klog.h>

int dmesg_main(int argc, char *argv[])
{
	char *size, *level;
	int flags = bb_getopt_ulflags(argc, argv, "cs:n:", &size, &level);

	if (flags & 4) {
		if(klogctl(8, NULL, bb_xgetlarg(level, 10, 0, 10)))
			bb_perror_msg_and_die("klogctl");
	} else {
		int len;
		char *buf;

		len = (flags & 2) ? bb_xgetlarg(size, 10, 2, INT_MAX) : 16384;
		buf = xmalloc(len);
		if (0 > (len = klogctl(3 + (flags & 1), buf, len)))
			bb_perror_msg_and_die("klogctl");
		write(1,buf,len);
		if (len && buf[len-1]!='\n') putchar('\n');
	}

	return 0;
}
