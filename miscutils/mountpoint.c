/* vi: set sw=4 ts=4: */
/*
 * mountpoint implementation for busybox
 *
 * Copyright (C) 2005 Bernhard Fischer
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 *
 * Based on sysvinit's mountpoint
 */

#include "busybox.h"

int mountpoint_main(int argc, char **argv)
{
	struct stat st;
	char *arg;
	int opt = getopt32(argc, argv, "qdx");
#define OPT_q (1)
#define OPT_d (2)
#define OPT_x (4)

	if (optind != argc - 1)
		bb_show_usage();

	arg = argv[optind];

	if ( (opt & OPT_x && stat(arg, &st) == 0) || (lstat(arg, &st) == 0) ) {
		if (opt & OPT_x) {
			if (S_ISBLK(st.st_mode)) {
				printf("%u:%u\n", major(st.st_rdev),
							minor(st.st_rdev));
				return EXIT_SUCCESS;
			} else {
				if (opt & OPT_q)
					putchar('\n');
				else
					bb_error_msg("%s: not a block device", arg);
			}
			return EXIT_FAILURE;
		} else
		if (S_ISDIR(st.st_mode)) {
			dev_t st_dev = st.st_dev;
			ino_t st_ino = st.st_ino;
			char *p = xasprintf("%s/..", arg);

			if (stat(p, &st) == 0) {
				int ret = (st_dev != st.st_dev) ||
					(st_dev == st.st_dev && st_ino == st.st_ino);
				if (opt & OPT_d)
					printf("%u:%u\n", major(st_dev), minor(st_dev));
				else if (!(opt & OPT_q))
					printf("%s is %sa mountpoint\n", arg, ret?"":"not ");
				return !ret;
			}
		} else {
			if (!(opt & OPT_q))
				bb_error_msg("%s: not a directory", arg);
			return EXIT_FAILURE;
		}
	}
	if (!(opt & OPT_q))
		bb_perror_msg("%s", arg);
	return EXIT_FAILURE;
}
