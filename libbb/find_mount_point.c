/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include "libbb.h"
#include <mntent.h>

/*
 * Given a block device, find the mount table entry if that block device
 * is mounted.
 *
 * Given any other file (or directory), find the mount table entry for its
 * filesystem.
 */
struct mntent* FAST_FUNC find_mount_point(const char *name)
{
	struct stat s;
	dev_t mountDevice;
	FILE *mountTable;
	struct mntent *mountEntry;

	if (stat(name, &s) != 0)
		return NULL;

	if (S_ISBLK(s.st_mode))
		mountDevice = s.st_rdev;
	else
		mountDevice = s.st_dev;


	mountTable = setmntent(bb_path_mtab_file, "r");
	if (!mountTable)
		return 0;

	while ((mountEntry = getmntent(mountTable)) != NULL) {
		/* rootfs mount in Linux 2.6 exists always,
		 * and it makes sense to always ignore it.
		 * Otherwise people can't reference their "real" root! */
		if (strcmp(mountEntry->mnt_fsname, "rootfs") == 0)
			continue;

		if (strcmp(name, mountEntry->mnt_dir) == 0
		 || strcmp(name, mountEntry->mnt_fsname) == 0
		) { /* String match. */
			break;
		}
		/* Match the device. */
		if (stat(mountEntry->mnt_fsname, &s) == 0 && s.st_rdev == mountDevice)
			break;
		/* Match the directory's mount point. */
		if (stat(mountEntry->mnt_dir, &s) == 0 && s.st_dev == mountDevice)
			break;
	}
	endmntent(mountTable);
	return mountEntry;
}
