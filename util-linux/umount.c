#include "internal.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <mntent.h>
#include <sys/mount.h>

const char	umount_usage[] = "umount {filesystem|directory}\n"
"\tumount -a\n"
"\n"
"\tUnmount a filesystem.\n"
"\t-a:\tUnmounts all mounted filesystems.\n";

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
	struct mntent	entries[100];
	int		count = 0;
	FILE *		mountTable = setmntent("/etc/mtab", "r");
	struct mntent *	m;

	if ( mountTable == 0
	 && (mountTable = setmntent("/proc/mounts", "r")) == 0 ) {
		name_and_error("/etc/mtab");
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
	if ( (mountTable = setmntent("/etc/mtab", "w")) ) {
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
		name_and_error("/etc/mtab");
}

static int
umount_all(int noMtab)
{
	struct mntent	entries[100];
	int				count = 0;
	FILE *			mountTable = setmntent("/etc/mtab", "r");
	struct mntent *	m;
	int				status = 0;

	if ( mountTable == 0
	 && (mountTable = setmntent("/proc/mounts", "r")) == 0 ) {
		name_and_error("/etc/mtab");
		return 1;
	}

	while ( (m = getmntent(mountTable)) != 0 ) {
		entries[count].mnt_fsname = stralloc(m->mnt_fsname);
		count++;
	}
	endmntent(mountTable);

	while ( count > 0 ) {
		int result = umount(entries[--count].mnt_fsname) == 0;
		/* free(entries[count].mnt_fsname); */
		if ( result ) {
			if ( !noMtab )
				erase_mtab(entries[count].mnt_fsname);
		}
		else {
			status = 1;
			name_and_error(entries[count].mnt_fsname);
		}
	}
	return status;
}

extern int
do_umount(const char * name, int noMtab)
{
	if ( umount(name) == 0 ) {
		if ( !noMtab )
			erase_mtab(name);
		return 0;
	}
	return 1;
}

extern int
umount_main(struct FileInfo * i, int argc, char * * argv)
{
	int	noMtab = 0;

	if ( argv[1][0] == '-' ) {
		switch ( argv[1][1] ) {
		case 'a':
			return umount_all(noMtab);
		case 'n':
			noMtab = 1;
			break;
		default:
			usage(umount_usage);
			return 1;
		}
	}
	if ( do_umount(argv[1],noMtab) != 0 ) {
		fprintf(stderr, "%s: %s.\n", argv[1], strerror(errno));
		return 1;
	}
	return 0;
}
