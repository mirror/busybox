/* vi: set sw=4 ts=4: */
/*
 * Mini swapon/swapoff implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and Erik Andersen
 * Copyright (C) 1999,2000,2001 by Erik Andersen <andersee@debian.org>
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

#if __GNU_LIBRARY__ < 5
/* libc5 doesn't have sys/swap.h, define these here. */ 
extern int swapon (__const char *__path, int __flags);
extern int swapoff (__const char *__path);
#else
#include <sys/swap.h>
#endif

#include "busybox.h"

static int whichApp;

static const int SWAPON_APP = 1;
static const int SWAPOFF_APP = 2;


static void swap_enable_disable(char *device)
{
	int status;

	if (whichApp == SWAPON_APP)
		status = swapon(device, 0);
	else
		status = swapoff(device);

	if (status != 0)
		perror_msg_and_die(applet_name);
}

static void do_em_all(void)
{
	struct mntent *m;
	FILE *f = setmntent("/etc/fstab", "r");

	if (f == NULL)
		perror_msg_and_die("/etc/fstab");
	while ((m = getmntent(f)) != NULL) {
		if (strcmp(m->mnt_type, MNTTYPE_SWAP)==0) {
			swap_enable_disable(m->mnt_fsname);
		}
	}
	endmntent(f);
	exit(EXIT_SUCCESS);
}


extern int swap_on_off_main(int argc, char **argv)
{
	if (strcmp(applet_name, "swapon") == 0) {
		whichApp = SWAPON_APP;
	} else {
		whichApp = SWAPOFF_APP;
	}

	if (argc != 2) {
		goto usage_and_exit;
	}
	argc--;
	argv++;

	/* Parse any options */
	while (**argv == '-') {
		while (*++(*argv))
			switch (**argv) {
			case 'a':
				{
					struct stat statBuf;

					if (stat("/etc/fstab", &statBuf) < 0)
						error_msg_and_die("/etc/fstab file missing");
				}
				do_em_all();
				break;
			default:
				goto usage_and_exit;
			}
	}
	swap_enable_disable(*argv);
	return EXIT_SUCCESS;

  usage_and_exit:
	show_usage();
}
