/* vi: set sw=4 ts=4: */
/*
 *
 * dmesg - display/control kernel ring buffer.
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 * Copyright 2006 Bernhard Fischer <rep.nop@aon.at>
 *
 * Licensed under GPLv2, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include <unistd.h>
#include <sys/klog.h>

int dmesg_main(int argc, char *argv[])
{
	char *size, *level;
	int flags = getopt32(argc, argv, "cs:n:", &size, &level);

	if (flags & 4) {
		if (klogctl(8, NULL, xatoul_range(level, 0, 10)))
			bb_perror_msg_and_die("klogctl");
	} else {
		int len;
		char *buf;

		len = (flags & 2) ? xatoul_range(size, 2, INT_MAX) : 16384;
		buf = xmalloc(len);
		if (0 > (len = klogctl(3 + (flags & 1), buf, len)))
			bb_perror_msg_and_die("klogctl");

		// Skip <#> at the start of lines, and make sure we end with a newline.

		if (ENABLE_FEATURE_DMESG_PRETTY) {
			int last = '\n';
			int in;

			for (in = 0; in<len;) {
				if (last == '\n' && buf[in] == '<') in += 3;
				else putchar(last = buf[in++]);
			}
			if (last != '\n') putchar('\n');
		} else {
			write(1,buf,len);
			if (len && buf[len-1]!='\n') putchar('\n');
		}

		if (ENABLE_FEATURE_CLEAN_UP) free(buf);
	}

	return 0;
}
