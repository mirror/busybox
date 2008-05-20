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

#include <sys/klog.h>
#include "libbb.h"

int dmesg_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int dmesg_main(int argc ATTRIBUTE_UNUSED, char **argv)
{
	int len;
	char *buf;
	char *size, *level;
	unsigned flags = getopt32(argv, "cs:n:", &size, &level);
	enum {
		OPT_c = 1<<0,
		OPT_s = 1<<1,
		OPT_n = 1<<2
	};

	if (flags & OPT_n) {
		if (klogctl(8, NULL, xatoul_range(level, 0, 10)))
			bb_perror_msg_and_die("klogctl");
		return EXIT_SUCCESS;
	}

	len = (flags & OPT_s) ? xatoul_range(size, 2, INT_MAX) : 16384;
	buf = xmalloc(len);
	len = klogctl(3 + (flags & OPT_c), buf, len);
	if (len < 0)
		bb_perror_msg_and_die("klogctl");
	if (len == 0)
		return EXIT_SUCCESS;

	/* Skip <#> at the start of lines, and make sure we end with a newline. */

	if (ENABLE_FEATURE_DMESG_PRETTY) {
		int last = '\n';
		int in = 0;

		do {
			if (last == '\n' && buf[in] == '<')
				in += 3;
			else {
				last = buf[in++];
				bb_putchar(last);
			}
		} while (in < len);
		if (last != '\n')
			bb_putchar('\n');
	} else {
		full_write(STDOUT_FILENO, buf, len);
		if (buf[len-1] != '\n')
			bb_putchar('\n');
	}

	if (ENABLE_FEATURE_CLEAN_UP) free(buf);

	return EXIT_SUCCESS;
}
