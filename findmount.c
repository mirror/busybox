#include "internal.h"
#include <stdio.h>
#include <mntent.h>
#include <sys/stat.h>

/*
 * Given a block device, find the mount table entry if that block device
 * is mounted.
 *
 * Given any other file (or directory), find the mount table entry for its
 * filesystem.
 */
extern struct mntent *
findMountPoint(const char * name, const char * table)
{
	struct stat	s;
	dev_t			mountDevice;
	FILE *			mountTable;
	struct mntent *	mountEntry;

	if ( stat(name, &s) != 0 )
		return 0;

	if ( (s.st_mode & S_IFMT) == S_IFBLK )
		mountDevice = s.st_rdev;
	else
		mountDevice = s.st_dev;

	
	if ( (mountTable = setmntent(table, "r")) == 0 )
		return 0;

	while ( (mountEntry = getmntent(mountTable)) != 0 ) {
		if ( strcmp(name, mountEntry->mnt_dir) == 0
		 || strcmp(name, mountEntry->mnt_fsname) == 0 )	/* String match. */
			break;
		if ( stat(mountEntry->mnt_fsname, &s) == 0
		 && s.st_rdev == mountDevice )	/* Match the device. */
				break;
		if ( stat(mountEntry->mnt_dir, &s) == 0
		 && s.st_dev == mountDevice )	/* Match the directory's mount point. */
			break;
	}
	endmntent(mountTable);
	return mountEntry;
}
