/*
 * Mini df implementation for busybox
 *
 * Copyright (C) 1999 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
 * based on original code by (I think) Bruce Perens <bruce@pixar.com>.
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
#include <mntent.h>
#include <sys/stat.h>
#include <sys/vfs.h>
#include <fstab.h>

static const char df_usage[] = "df [filesystem ...]\n"
    "\n" "\tPrint the filesystem space used and space available.\n";

extern const char mtab_file[]; /* Defined in utility.c */

static int df(char *device, const char *mountPoint)
{
    struct statfs s;
    long blocks_used;
    long blocks_percent_used;
    struct fstab* fstabItem;

    if (statfs(mountPoint, &s) != 0) {
	perror(mountPoint);
	return 1;
    }

    if (s.f_blocks > 0) {
	blocks_used = s.f_blocks - s.f_bfree;
	blocks_percent_used = (long)
	    (blocks_used * 100.0 / (blocks_used + s.f_bavail) + 0.5);
	/* Note that if /etc/fstab is missing, libc can't fix up /dev/root for us */
	if (strcmp (device, "/dev/root") == 0) {
	    fstabItem = getfsfile ("/");
	    if (fstabItem != NULL)
		device = fstabItem->fs_spec;
	}
	printf("%-20s %9ld %9ld %9ld %3ld%% %s\n",
	       device,
	       (long) (s.f_blocks * (s.f_bsize / 1024.0)),
	       (long) ((s.f_blocks - s.f_bfree) * (s.f_bsize / 1024.0)),
	       (long) (s.f_bavail * (s.f_bsize / 1024.0)),
	       blocks_percent_used, mountPoint);

    }

    return 0;
}

extern int df_main(int argc, char **argv)
{
    printf("%-20s %-14s %s %s %s %s\n", "Filesystem",
	   "1k-blocks", "Used", "Available", "Use%", "Mounted on");

    /* Only compiled in if BB_MTAB is not defined */
    whine_if_fstab_is_missing();

    if (argc > 1) {
	struct mntent *mountEntry;
	int status;

	while (argc > 1) {
	    if ((mountEntry = findMountPoint(argv[1], mtab_file)) ==
		0) {
		fprintf(stderr, "%s: can't find mount point.\n", argv[1]);
		return 1;
	    }
	    status = df(mountEntry->mnt_fsname, mountEntry->mnt_dir);
	    if (status != 0)
		return status;
	    argc--;
	    argv++;
	}
	return 0;
    } else {
	FILE *mountTable;
	struct mntent *mountEntry;

	mountTable = setmntent(mtab_file, "r");
	if (mountTable == 0) {
	    perror(mtab_file);
	    exit(FALSE);
	}

	while ((mountEntry = getmntent(mountTable))) {
	    int status = df(mountEntry->mnt_fsname, mountEntry->mnt_dir);
	    if (status)
		return status;
	}
	endmntent(mountTable);
    }

    return 0;
}
