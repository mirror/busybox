/* vi: set sw=4 ts=4: */
/*
 * Mini swapon/swapoff implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <stdio.h>
#include <mntent.h>
#include <dirent.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/swap.h>

#include "busybox.h"

static int swap_enable_disable(const char *device)
{
	int status;
	struct stat st;

	if (stat(device, &st) < 0) {
		bb_perror_msg_and_die("cannot stat %s", device);
	}

	/* test for holes */
	if (S_ISREG(st.st_mode)) {
		if (st.st_blocks * 512 < st.st_size) {
			bb_error_msg_and_die("swap file has holes");
		}
	}

	if (bb_applet_name[5] == 'n')
		status = swapon(device, 0);
	else
		status = swapoff(device);

	if (status != 0) {
		bb_perror_msg("%s", device);
		return EXIT_FAILURE;
	}
	/*printf("%s: %s\n", bb_applet_name, device);*/
	return EXIT_SUCCESS;
}

static int do_em_all(void)
{
	struct mntent *m;
	FILE *f = setmntent("/etc/fstab", "r");
	int err = 0;

	if (f == NULL)
		bb_perror_msg_and_die("/etc/fstab");
	while ((m = getmntent(f)) != NULL) {
		if (strcmp(m->mnt_type, MNTTYPE_SWAP)==0) {
			if(swap_enable_disable(m->mnt_fsname) == EXIT_FAILURE)
				err++;
		}
	}
	endmntent(f);
	return err;
}

#define DO_ALL      1

extern int swap_on_off_main(int argc, char **argv)
{
	unsigned long opt = bb_getopt_ulflags (argc, argv, "a");
	
	if (argc != 2) {
		bb_show_usage();
	}
	
	if (opt & DO_ALL)
		return do_em_all();
	
	return swap_enable_disable(argv[1]);
}
