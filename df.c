#include "internal.h"
#include <stdio.h>
#include <mntent.h>
#include <sys/stat.h>
#include <sys/vfs.h>

const char	df_usage[] = "df [filesystem ...]\n"
"\n"
"\tPrint the filesystem space used and space available.\n";


static int
df(const char * device, const char * mountPoint)
{
	struct statfs	s;
	long		blocks_used;
	long		blocks_percent_used;

	if ( statfs(mountPoint, &s) != 0 ) {
		name_and_error(mountPoint);
		return 1;
	}
	
	if ( s.f_blocks > 0 ) {
		blocks_used = s.f_blocks - s.f_bfree;
		blocks_percent_used = (long)
		 (blocks_used * 100.0 / (blocks_used + s.f_bavail) + 0.5);

/*
		printf(
		 "%-20s %7ld %7ld  %7ld  %5ld%%   %s\n"
		,device
		,s.f_blocks
		,s.f_blocks - s.f_bfree
		,s.f_bavail
		,blocks_percent_used
		,mountPoint);
*/

		printf(
		       "%-20s %7.0f %7.0f  %7.0f  %5ld%%   %s\n"
		       ,device
		       ,s.f_blocks * (s.f_bsize / 1024.0)
		       ,(s.f_blocks - s.f_bfree)  * (s.f_bsize / 1024.0)
		       ,s.f_bavail * (s.f_bsize / 1024.0)
		       ,blocks_percent_used
		       ,mountPoint);

	}

	return 0;
}

extern int
df_main(struct FileInfo * i, int argc, char * * argv)
{
	static const char header[] =
	 "Filesystem         1024-blocks  Used Available Capacity Mounted on\n";
	printf(header);

	if ( argc > 1 ) {
		struct mntent *	mountEntry;
		int				status;

		while ( argc > 1 ) {
			if ( (mountEntry = findMountPoint(argv[1], "/etc/mtab")) == 0
			 && (mountEntry = findMountPoint(argv[1], "/proc/mounts")) == 0 )
			{
				fprintf(stderr, "%s: can't find mount point.\n"
				,argv[1]);
				return 1;
			}
			status = df(mountEntry->mnt_fsname, mountEntry->mnt_dir);
			if ( status != 0 )
				return status;
			argc--;
			argv++;
		}
		return 0;
	}
	else {
		FILE *		mountTable;
		struct mntent *	mountEntry;

		if ( (mountTable = setmntent("/etc/mtab", "r")) == 0
		 && (mountTable = setmntent("/proc/mounts", "r")) == 0
		) {
			name_and_error("/etc/mtab");
			return 1;
		}

		while ( (mountEntry = getmntent(mountTable)) != 0 ) {
			int status = df(
			 mountEntry->mnt_fsname
			,mountEntry->mnt_dir);
			if ( status != 0 )
				return status;
		}
		endmntent(mountTable);
	}
	
	return 0;
}
