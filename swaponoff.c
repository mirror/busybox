/* vi: set sw=4 ts=4: */
/*
 * Mini swapon/swapoff implementation for busybox
 *
 *
 * Copyright (C) 1999,2000 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
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

#include "internal.h"
#include <stdio.h>
#include <sys/mount.h>
#include <sys/swap.h>
#include <mntent.h>
#include <dirent.h>
#include <fstab.h>
#include <errno.h>


static int whichApp;
static const char *appName;

static const char swapoff_usage[] =
	"swapoff [OPTION] [device]\n\n"
	"Stop swapping virtual memory pages on the given device.\n\n"
	"Options:\n"
	"\t-a\tStop swapping on all swap devices\n";

static const char swapon_usage[] =
	"swapon [OPTION] [device]\n\n"
	"Start swapping virtual memory pages on the given device.\n\n"
	"Options:\n"
	"\t-a\tStart swapping on all swap devices\n";


#define SWAPON_APP   1
#define SWAPOFF_APP  2


static void swap_enable_disable(char *device)
{
	int status;

	if (whichApp == SWAPON_APP)
		status = swapon(device, 0);
	else
		status = swapoff(device);

	if (status != 0) {
		perror(appName);
		exit(FALSE);
	}
}

static void do_em_all()
{
	struct mntent *m;
	FILE *f = setmntent("/etc/fstab", "r");

	if (f == NULL) {
		perror("/etc/fstab");
		exit(FALSE);
	}
	while ((m = getmntent(f)) != NULL) {
		if (!strstr(m->mnt_type, MNTTYPE_SWAP)) {
			swap_enable_disable(m->mnt_fsname);
		}
	}
	endmntent(f);
	exit(TRUE);
}


extern int swap_on_off_main(int argc, char **argv)
{
	if (strcmp(*argv, "swapon") == 0) {
		appName = *argv;
		whichApp = SWAPON_APP;

	} else {
		appName = *argv;
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
						fatalError("/etc/fstab file missing\n");
				}
				do_em_all();
				break;
			default:
				goto usage_and_exit;
			}
	}
	swap_enable_disable(*argv);
	exit(TRUE);

  usage_and_exit:
	usage((whichApp == SWAPON_APP) ? swapon_usage : swapoff_usage);
	exit(FALSE);
}
