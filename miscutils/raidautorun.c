/* vi: set sw=4 ts=4: */
/*
 * raidautorun implementation for busybox
 *
 * Copyright (C) 2006 Bernhard Fischer
 *
 * Licensed under the GPL v2 or later, see the file LICENSE in this tarball.
 *
 */

#include "libbb.h"

#include <linux/major.h>
#include <linux/raid/md_u.h>

int raidautorun_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int raidautorun_main(int argc, char **argv)
{
	if (argc != 2)
		bb_show_usage();

	xioctl(xopen(argv[1], O_RDONLY), RAID_AUTORUN, NULL);

	return EXIT_SUCCESS;
}
