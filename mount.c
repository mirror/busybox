/*
   3/21/1999	Charles P. Wright <cpwright@cpwright.com>
		searches through fstab when -a is passed
		will try mounting stuff with all fses when passed -t auto

	1999-04-17	Dave Cinege...Rewrote -t auto. Fixed ro mtab.
*/

#include "internal.h"
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <mntent.h>
#include <sys/mount.h>
#include <ctype.h>

const char	mount_usage[] = "mount\n"
"\t\tmount [flags] special-device directory\n"
"\n"
"Flags:\n"
"\t-a:\tMount all file systems in fstab.\n"
"\t-f:\t\"Fake\" mount. Add entry to mount table but don't mount it.\n"
"\t-n:\tDon't write a mount table entry.\n"
"\t-o option:\tOne of many filesystem options, listed below.\n"
"\t-r:\tMount the filesystem read-only.\n"
"\t-t filesystem-type:\tSpecify the filesystem type.\n"
"\t-w:\tMount for reading and writing (default).\n"
"\n"
"Options for use with the \"-o\" flag:\n"
"\tasync / sync:\tWrites are asynchronous / synchronous.\n"
"\tdev / nodev:\tAllow use of special device files / disallow them.\n"
"\texec / noexec:\tAllow use of executable files / disallow them.\n"
"\tsuid / nosuid:\tAllow set-user-id-root programs / disallow them.\n"
"\tremount: Re-mount a currently-mounted filesystem, changing its flags.\n"
"\tro / rw: Mount for read-only / read-write.\n"
"\t"
"There are EVEN MORE flags that are specific to each filesystem.\n"
"You'll have to see the written documentation for those.\n";

struct mount_options {
	const char *	name;
	unsigned long	and;
	unsigned long	or;
};

static const struct mount_options	mount_options[] = {
	{	"async",	~MS_SYNCHRONOUS,0		},
	{	"defaults",	~0,		0		},
	{	"dev",		~MS_NODEV,	0		},
	{	"exec",		~MS_NOEXEC,	0		},
	{	"nodev",	~0,		MS_NODEV	},
	{	"noexec",	~0,		MS_NOEXEC	},
	{	"nosuid",	~0,		MS_NOSUID	},
	{	"remount",	~0,		MS_REMOUNT	},
	{	"ro",		~0,		MS_RDONLY	},
	{	"rw",		~MS_RDONLY,	0		},
	{	"suid",		~MS_NOSUID,	0		},
	{	"sync",		~0,		MS_SYNCHRONOUS	},
	{	0,		0,		0		}
};

static void
show_flags(unsigned long flags, char * buffer)
{
	const struct mount_options *	f = mount_options;
	while ( f->name ) {
		if ( flags & f->and ) {
			int	length = strlen(f->name);
			memcpy(buffer, f->name, length);
			buffer += length;
			*buffer++ = ',';
			*buffer = '\0';
		}
		f++;
	}
}

static void
one_option(
 char *			option
,unsigned long *	flags
,char *			data)
{
	const struct mount_options *	f = mount_options;

	while ( f->name != 0 ) {
		if ( strcasecmp(f->name, option) == 0 ) {
			*flags &= f->and;
			*flags |= f->or;
			return;
		}
		f++;
	}
	if ( *data ) {
		data += strlen(data);
		*data++ = ',';
	}
	strcpy(data, option);
}

static void
parse_mount_options(
 char *			options
,unsigned long *	flags
,char *			data)
{
	while ( *options ) {
		char * comma = strchr(options, ',');
		if ( comma )
			*comma = '\0';
		one_option(options, flags, data);
		if ( comma ) {
			*comma = ',';
			options = ++comma;
		}
		else
			break;
	}
}

int
mount_one(
 char *		blockDevice
,char *		directory
,char *		filesystemType
,unsigned long	flags
,char *		string_flags
,int		noMtab
,int		fake)
{
	int	error = 0;
	int 	status = 0;

	char	buf[255];
		
	if (!fake) {
		if (*filesystemType == 'a') { 			//Will fail on real FS starting with 'a'

			FILE	*f = fopen("/proc/filesystems", "r");

			if (f == NULL)	return 1;

			while (fgets(buf, sizeof(buf), f) != NULL) {
				filesystemType = buf;
				if (*filesystemType == '\t') {  // Not a nodev filesystem
					
					while (*filesystemType && *filesystemType != '\n')	filesystemType++;
					*filesystemType = '\0';
						
					filesystemType = buf;
					filesystemType++;	//hop past tab
					
					status = mount(blockDevice, directory, filesystemType,
							flags|MS_MGC_VAL ,string_flags);
					error = errno;

					if (status == 0) break;
				}
			}
			fclose(f);
		} else {	

			status = mount( blockDevice, directory, filesystemType,
					flags|MS_MGC_VAL ,string_flags);
			error = errno;
		}
	}

	if ( status == 0 ) {
		char *	s = &string_flags[strlen(string_flags)];
		FILE *	mountTable;
		if ( s != string_flags ) {
			*s++ = ',';
			show_flags(flags, s);
		}
		if ( !noMtab && (mountTable = setmntent("/etc/mtab", "a+")) ) {
			int	length = strlen(directory);
			struct mntent	m;

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
		return 0;
	} else {
		fprintf(stderr, "Mount %s", blockDevice);
		if ( filesystemType && *filesystemType )
			fprintf(stderr, " (type %s)", filesystemType);

		fprintf(
		 stderr
		," on %s: "
		,directory);

		switch ( error ) {
		case EPERM:
			if (geteuid() == 0)
				fprintf(
				 stderr
				,"mount point %s is not a directory"
				,blockDevice);
			else
				fprintf(
				 stderr
				,"must be superuser to use mount");
			break;
		case EBUSY:
			fprintf(
			 stderr
			,"%s already mounted or %s busy"
			,blockDevice
			,directory);
			break;
		case ENOENT:
		{
			struct stat statbuf;
			if ( stat(directory, &statbuf) != 0 )
				fprintf(
				 stderr
				,"directory %s does not exist"
				,directory);
			else if ( stat(blockDevice, &statbuf) != 0 )
				fprintf(
				 stderr
				,"block device %s does not exist"
				,blockDevice);
			else
				fprintf(
				 stderr
				,"%s is not mounted on %s, but the mount table says it is."
				,blockDevice
				,directory);
			break;
		}
		case ENOTDIR:
			fprintf(
			 stderr
			,"%s is not a directory"
			,directory);
			break;
		case EINVAL:
			fprintf(
			 stderr
			,"wrong filesystem type, or bad superblock on %s"
			,blockDevice);
			break;
		case EMFILE:
			fprintf(stderr, "mount table full");
			break;
		case EIO:
			fprintf(
			 stderr
			,"I/O error reading %s"
			,blockDevice);
			break;
		case ENODEV:
		{
			FILE *	f = fopen("/proc/filesystems", "r");

			fprintf(
			 stderr
			,"filesystem type %s not in kernel.\n"
			,filesystemType);
			fprintf(stderr, "Do you need to load a module?\n");
			if ( f ) {
				char	buf[100];

				fprintf(
				 stderr
				,"Here are the filesystem types the kernel"
				 " can mount:\n");
				while ( fgets(buf, sizeof(buf), f) != 0 )
					fprintf(stderr, "\t%s", buf);
				fclose(f);
			}
			break;
		}
		case ENOTBLK:
			fprintf(
			 stderr
			,"%s is not a block device"
			,blockDevice);
			break;
		case ENXIO:
			fprintf(
			 stderr
			,"%s is not a valid block device"
			,blockDevice);
			break;
		default:
			fputs(strerror(errno), stderr);
		}
		putc('\n', stderr);
		return -1;
	}
}

extern int
mount_main(struct FileInfo * i, int argc, char * * argv)
{
	char	string_flags[1024];
	unsigned long	flags = 0;
	char *	filesystemType = "auto";
	int		fake = 0;
	int		noMtab = 0;
	int 		all = 0;
	
	*string_flags = '\0';

	if ( argc == 1 ) {
		FILE *	mountTable;
		if ( (mountTable = setmntent("/etc/mtab", "r")) ) {
			struct mntent * m;
			while ( (m = getmntent(mountTable)) != 0 ) {
				printf(
				 "%s on %s type %s (%s)\n"
				,m->mnt_fsname
				,m->mnt_dir
				,m->mnt_type
				,m->mnt_opts);
			}
			endmntent(mountTable);
		}
		return 0;
	}

	while ( argc >= 2 && argv[1][0] == '-' ) {
		switch ( argv[1][1] ) {
		case 'f':
			fake = 1;
			break;
		case 'n':
			noMtab = 1;
			break;
		case 'o':
			if ( argc < 3 ) {
				usage(mount_usage);
				return 1;
			}
			parse_mount_options(argv[2], &flags, string_flags);
			argc--;
			argv++;
			break;
		case 'r':
			flags |= MS_RDONLY;
			break;
		case 't':
			if ( argc < 3 ) {
				usage(mount_usage);
				return 1;
			}
			filesystemType = argv[2];
			argc--;
			argv++;
			break;
		case 'v':
			break;
		case 'w':
			flags &= ~MS_RDONLY;
			break;
		case 'a':
			all = 1;
			break;
		default:
			usage(mount_usage);
			return 1;
		}
		argc--;
		argv++;
	}

	if (all == 1) {
		struct 	mntent *m;
		FILE	*f = setmntent("/etc/fstab", "r");

		if (f == NULL)	{
			return 1;
		}

		// FIXME: Combine read routine (make new function) with unmount_all to save space.

		while ((m = getmntent(f)) != NULL) {	
			// If the file system isn't noauto, and isn't mounted on /, mount it
			if ((!strstr(m->mnt_opts, "noauto")) && (m->mnt_dir[1] != '\0') 
				&& !((m->mnt_type[0] == 's') && (m->mnt_type[1] == 'w')) 
				&& !((m->mnt_type[0] == 'n') && (m->mnt_type[1] == 'f'))) {
				mount_one(m->mnt_fsname, m->mnt_dir, m->mnt_type, flags, m->mnt_opts, noMtab, fake);	
			}
		}

		endmntent(f);
	} else {
		if ( argc >= 3 ) {
			if ( mount_one( argv[1], argv[2], filesystemType, flags, string_flags, noMtab, fake) == 0 )
				return 0;
			else
				return 1;
		} else {
			usage(mount_usage);
			return 1;
		}
	}
	return 0;
}
