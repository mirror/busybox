/* vi: set sw=4 ts=4: */
/*
 * dmesg - display/control kernel ring buffer.
 *
 * Copyright 2006 Rob Landley <rob@landley.net>
 * Copyright 2006 Erik Andersen <andersen@codepoet.org>
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
		if (klogctl(8, NULL, bb_xgetlarg(level, 10, 0, 10)))
			bb_perror_msg_and_die("klogctl");
	} else {
		int len;
		char *buf;

		len = (flags & 2) ? bb_xgetlarg(size, 10, 2, INT_MAX) : 16384;
		buf = xmalloc(len);
		if (0 > (len = klogctl(3 + (flags & 1), buf, len)))
			bb_perror_msg_and_die("klogctl");

#ifdef CONFIG_FEATURE_DMESG_PRETTY
		{
			char newline = '\n';
			int i;
			for (i=0; i<len; ++i) {
				if (newline == '\n' && buf[i] == '<')
					i += 3; /* skip <#> */
				putchar(newline=buf[i]);
			}
			if (newline != '\n') putchar('\n');
		}
#else
		write(1, buf, len);
		if (len && buf[len-1]!='\n') putchar('\n');
#endif

		if (ENABLE_FEATURE_CLEAN_UP) free(buf);
	}

	return 0;
}
