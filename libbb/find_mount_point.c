/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
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
 */

#include <stdio.h>
#include <string.h>
#include "libbb.h"


#include <mntent.h>
/*
 * Given a block device, find the mount table entry if that block device
 * is mounted.
 *
 * Given any other file (or directory), find the mount table entry for its
 * filesystem.
 */
extern struct mntent *find_mount_point(const char *name, const char *table)
{
	struct stat s;
	dev_t mountDevice;
	FILE *mountTable;
	struct mntent *mountEntry;

	if (stat(name, &s) != 0)
		return 0;

	if ((s.st_mode & S_IFMT) == S_IFBLK)
		mountDevice = s.st_rdev;
	else
		mountDevice = s.st_dev;


	if ((mountTable = setmntent(table ? : bb_path_mtab_file, "r")) == 0)
		return 0;

	while ((mountEntry = getmntent(mountTable)) != 0) {
		if (strcmp(name, mountEntry->mnt_dir) == 0
			|| strcmp(name, mountEntry->mnt_fsname) == 0)	/* String match. */
			break;
		if (stat(mountEntry->mnt_fsname, &s) == 0 && s.st_rdev == mountDevice)	/* Match the device. */
			break;
		if (stat(mountEntry->mnt_dir, &s) == 0 && s.st_dev == mountDevice)	/* Match the directory's mount point. */
			break;
	}
	endmntent(mountTable);
	return mountEntry;
}


/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
