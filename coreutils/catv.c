/* vi: set sw=4 ts=4: */
/*
 * cat -v implementation for busybox
 *
 * Copyright (C) 2006 Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this source tree.
 */

/* See "Cat -v considered harmful" at
 * http://cm.bell-labs.com/cm/cs/doc/84/kp.ps.gz */

//usage:#define catv_trivial_usage
//usage:       "[-etv] [FILE]..."
//usage:#define catv_full_usage "\n\n"
//usage:       "Display nonprinting characters as ^x or M-x\n"
//usage:     "\n	-e	End each line with $"
//usage:     "\n	-t	Show tabs as ^I"
//usage:     "\n	-v	Don't use ^x or M-x escapes"

#include "libbb.h"

int catv_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int catv_main(int argc UNUSED_PARAM, char **argv)
{
	int retval = EXIT_SUCCESS;
	int fd;
	unsigned opts;
	int flags = 0;

	opts = getopt32(argv, "etv");
#define CATV_OPT_e (1<<0)
#define CATV_OPT_t (1<<1)
#define CATV_OPT_v (1<<2)
	argv += optind;
	if (opts & (CATV_OPT_e | CATV_OPT_t))
		opts &= ~CATV_OPT_v;
	if (opts & CATV_OPT_e)
		flags |= VISIBLE_ENDLINE;
	if (opts & CATV_OPT_t)
		flags |= VISIBLE_SHOW_TABS;

	/* Read from stdin if there's nothing else to do. */
	if (!argv[0])
		*--argv = (char*)"-";
	do {
		fd = open_or_warn_stdin(*argv);
		if (fd < 0) {
			retval = EXIT_FAILURE;
			continue;
		}
		for (;;) {
			int i, res;

#define read_buf bb_common_bufsiz1
			res = read(fd, read_buf, COMMON_BUFSIZE);
			if (res < 0)
				retval = EXIT_FAILURE;
			if (res <= 0)
				break;
			for (i = 0; i < res; i++) {
				unsigned char c = read_buf[i];
				if (opts & CATV_OPT_v) {
					putchar(c);
				} else {
					char buf[sizeof("M-^c")];
					visible(c, buf, flags);
					fputs(buf, stdout);
				}
			}
		}
		if (ENABLE_FEATURE_CLEAN_UP && fd)
			close(fd);
	} while (*++argv);

	fflush_stdout_and_exit(retval);
}
