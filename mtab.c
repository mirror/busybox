#include "internal.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <mntent.h>
#include <sys/mount.h>

extern const char mtab_file[]; /* Defined in utility.c */

static char *
stralloc(const char * string)
{
	int	length = strlen(string) + 1;
	char *	n = malloc(length);
	memcpy(n, string, length);
	return n;
}

extern void
erase_mtab(const char * name)
{
	struct mntent	entries[20];
	int	count = 0;
	FILE *mountTable = setmntent(mtab_file, "r");
	struct mntent *	m;

	if ( mountTable == 0
	 && (mountTable = setmntent("/proc/mounts", "r")) == 0 ) {
		perror(mtab_file);
		return;
	}

	while ( (m = getmntent(mountTable)) != 0 ) {
		entries[count].mnt_fsname = stralloc(m->mnt_fsname);
		entries[count].mnt_dir = stralloc(m->mnt_dir);
		entries[count].mnt_type = stralloc(m->mnt_type);
		entries[count].mnt_opts = stralloc(m->mnt_opts);
		entries[count].mnt_freq = m->mnt_freq;
		entries[count].mnt_passno = m->mnt_passno;
		count++;
	}
	endmntent(mountTable);
	if ( (mountTable = setmntent(mtab_file, "w")) ) {
		int	i;
		for ( i = 0; i < count; i++ ) {
			int result = ( strcmp(entries[i].mnt_fsname, name) == 0
			 || strcmp(entries[i].mnt_dir, name) == 0 );

			if ( result )
				continue;
			else
				addmntent(mountTable, &entries[i]);
		}
		endmntent(mountTable);
	}
	else if ( errno != EROFS )
		perror(mtab_file);
}

/*
 * Given a block device, find the mount table entry if that block device
 * is mounted.
 *
 * Given any other file (or directory), find the mount table entry for its
 * filesystem.
 */
static struct mntent *
findMountPoint(const char * name, const char * table)
{
    struct stat	s;
    dev_t mountDevice;
    FILE* mountTable;
    struct mntent* mountEntry;

    if ( stat(name, &s) != 0 )
	return 0;

    if ( (s.st_mode & S_IFMT) == S_IFBLK )
	mountDevice = s.st_rdev;
    else
	mountDevice = s.st_dev;

    
    if ( (mountTable = setmntent(table, "r")) == 0 )
	return 0;

    while ( (mountEntry = getmntent(mountTable)) != 0 ) {
	if ( strcmp(name, mountEntry->mnt_dir) == 0 || 
		strcmp(name, mountEntry->mnt_fsname) == 0 )	/* String match. */
	    break;
	if ( stat(mountEntry->mnt_fsname, &s) == 0 && 
		s.st_rdev == mountDevice )	/* Match the device. */
	    break;
	if ( stat(mountEntry->mnt_dir, &s) == 0 && 
		s.st_dev == mountDevice )	/* Match the directory's mount point. */
	    break;
    }
    endmntent(mountTable);
    return mountEntry;
}

extern void 
write_mtab(char* blockDevice, char* directory, 
	char* filesystemType, long flags, char* string_flags)
{
	FILE *mountTable = setmntent(mtab_file, "a+");
	struct mntent m;

	if ( mountTable == 0 ) {
		perror(mtab_file);
		return;
	}
	if (mountTable) {
	    int	length = strlen(directory);

	    if ( length > 1 && directory[length - 1] == '/' )
		    directory[length - 1] = '\0';

	    if ( filesystemType == 0 ) {
		    struct mntent *	p
		     = findMountPoint(blockDevice, "/proc/mounts");

		    if ( p && p->mnt_type )
			    filesystemType = p->mnt_type;
	    }
	    m.mnt_fsname = blockDevice;
	    m.mnt_dir = directory;
	    m.mnt_type = filesystemType ? filesystemType : "default";
	    
	    if (*string_flags) {
		    m.mnt_opts = string_flags;
	    } else {
		    if ( (flags | MS_RDONLY) == flags )
			    m.mnt_opts = "ro";
		    else	
			    m.mnt_opts = "rw";
	    }
	    
	    m.mnt_freq = 0;
	    m.mnt_passno = 0;
	    addmntent(mountTable, &m);
	    endmntent(mountTable);
    }
}


