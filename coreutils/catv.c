/* vi: set sw=4 ts=4: */
/*
 * cat -v implementation for busybox
 *
 * Copyright (C) 2006 Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* See "Cat -v considered harmful" at
 * http://cm.bell-labs.com/cm/cs/doc/84/kp.ps.gz */

#include "busybox.h"

int catv_main(int argc, char **argv)
{
	int retval = EXIT_SUCCESS, fd;
	unsigned flags;

	flags = getopt32(argc, argv, "etv");
#define CATV_OPT_e (1<<0)
#define CATV_OPT_t (1<<1)
#define CATV_OPT_v (1<<2)
	flags ^= CATV_OPT_v;

	argv += optind;
	do {
		/* Read from stdin if there's nothing else to do. */
		fd = 0;
		if (*argv && 0 > (fd = xopen(*argv, O_RDONLY)))
			retval = EXIT_FAILURE;
		else for (;;) {
			int i, res;

			res = read(fd, bb_common_bufsiz1, sizeof(bb_common_bufsiz1));
			if (res < 0)
				retval = EXIT_FAILURE;
			if (res < 1)
				break;
			for (i = 0; i < res; i++) {
				char c = bb_common_bufsiz1[i];

				if (c > 126 && (flags & CATV_OPT_v)) {
					if (c == 127) {
						printf("^?");
						continue;
					} else {
						printf("M-");
						c -= 128;
					}
				}
				if (c < 32) {
					if (c == 10) {
						if (flags & CATV_OPT_e)
							putchar('$');
					} else if (flags & (c==9 ? CATV_OPT_t : CATV_OPT_v)) {
						printf("^%c", c+'@');
						continue;
					}
				}
				putchar(c);
			}
		}
		if (ENABLE_FEATURE_CLEAN_UP && fd)
			close(fd);
	} while (*++argv);

	fflush_stdout_and_exit(retval);
}
