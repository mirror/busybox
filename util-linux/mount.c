/* vi: set sw=4 ts=4: */
/*
 * Mini mount implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2005-2006 by Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

/* todo:
 * bb_getopt_ulflags();
 */

/* Design notes: There is no spec for this.  Remind me to write one.

   mount_main() calls singlemount() which calls mount_it_now().

   mount_main() can loop through /etc/fstab for mount -a
   singlemount() can loop through /etc/filesystems for fstype detection.
   mount_it_now() does the actual mount.
*/

#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <mntent.h>
#include <ctype.h>
#include <sys/mount.h>
#include <fcntl.h>		// for CONFIG_FEATURE_MOUNT_LOOP
#include <sys/ioctl.h>  // for CONFIG_FEATURE_MOUNT_LOOP
#include "busybox.h"

// These two aren't always defined in old headers
#ifndef MS_BIND
#define MS_BIND		4096
#endif
#ifndef MS_MOVE
#define MS_MOVE		8192
#endif
#ifndef MS_SILENT
#define MS_SILENT	32768
#endif

// Not real flags, but we want to be able to check for this.
#define MOUNT_NOAUTO    (1<<29)
#define MOUNT_SWAP      (1<<30)
/* Standard mount options (from -o options or --options), with corresponding
 * flags */

struct {
	const char *name;
	long flags;
} static const mount_options[] = {
	// NOP flags.

	{"loop", 0},
	{"defaults", 0},
	{"quiet", 0},

	// vfs flags

	{"ro", MS_RDONLY},
	{"rw", ~MS_RDONLY},
	{"nosuid", MS_NOSUID},
	{"suid", ~MS_NOSUID},
	{"dev", ~MS_NODEV},
	{"nodev", MS_NODEV},
	{"exec", ~MS_NOEXEC},
	{"noexec", MS_NOEXEC},
	{"sync", MS_SYNCHRONOUS},
	{"async", ~MS_SYNCHRONOUS},
	{"atime", ~MS_NOATIME},
	{"noatime", MS_NOATIME},
	{"diratime", ~MS_NODIRATIME},
	{"nodiratime", MS_NODIRATIME},
	{"loud", ~MS_SILENT},

	// action flags

	{"remount", MS_REMOUNT},
	{"bind", MS_BIND},
	{"move", MS_MOVE},
	{"noauto",MOUNT_NOAUTO},
	{"swap",MOUNT_SWAP}
};

/* Append mount options to string */
static void append_mount_options(char **oldopts, char *newopts)
{
	if(*oldopts && **oldopts) {
		char *temp=bb_xasprintf("%s,%s",*oldopts,newopts);
		free(*oldopts);
		*oldopts=temp;
	} else {
		if (ENABLE_FEATURE_CLEAN_UP) free(*oldopts);
		*oldopts = bb_xstrdup(newopts);
	}
}

/* Use the mount_options list to parse options into flags.
 * Return list of unrecognized options in *strflags if strflags!=NULL */
static int parse_mount_options(char *options, char **unrecognized)
{
	int flags = MS_SILENT;

	// Loop through options
	for (;;) {
		int i;
		char *comma = strchr(options, ',');

		if (comma) *comma = 0;

		// Find this option in mount_options
		for (i = 0; i < (sizeof(mount_options) / sizeof(*mount_options)); i++) {
			if (!strcasecmp(mount_options[i].name, options)) {
				long fl = mount_options[i].flags;
				if(fl < 0) flags &= fl;
				else flags |= fl;
				break;
			}
		}
		// If unrecognized not NULL, append unrecognized mount options */
		if (unrecognized
				&& i == (sizeof(mount_options) / sizeof(*mount_options)))
		{
			// Add it to strflags, to pass on to kernel
			i = *unrecognized ? strlen(*unrecognized) : 0;
			*unrecognized = xrealloc(*unrecognized, i+strlen(options)+2);

			// Comma separated if it's not the first one
			if (i) (*unrecognized)[i++] = ',';
			strcpy((*unrecognized)+i, options);
		}

		// Advance to next option, or finish
		if(comma) {
			*comma = ',';
			options = ++comma;
		} else break;
	}

	return flags;
}

// Return a list of all block device backed filesystems

static llist_t *get_block_backed_filesystems(void)
{
	char *fs, *buf,
		 *filesystems[] = {"/etc/filesystems", "/proc/filesystems", 0};
	llist_t *list = 0;
	int i;
	FILE *f;

	for(i = 0; filesystems[i]; i++) {
		if(!(f = fopen(filesystems[i], "r"))) continue;

		for(fs = buf = 0; (fs = buf = bb_get_chomped_line_from_file(f));
			free(buf))
		{
			if(!strncmp(buf,"nodev",5) && isspace(buf[5])) continue;

			while(isspace(*fs)) fs++;
			if(*fs=='#' || *fs=='*') continue;
			if(!*fs) continue;

			list=llist_add_to_end(list,bb_xstrdup(fs));
		}
		if (ENABLE_FEATURE_CLEAN_UP) fclose(f);
	}

	return list;
}

llist_t *fslist = 0;

#if ENABLE_FEATURE_CLEAN_UP
static void delete_block_backed_filesystems(void)
{
	llist_free(fslist);
}
#else
void delete_block_backed_filesystems(void);
#endif

#if ENABLE_FEATURE_MTAB_SUPPORT
static int useMtab;
static int fakeIt;
#else
#define useMtab 0
#define fakeIt 0
#endif

// Perform actual mount of specific filesystem at specific location.

static int mount_it_now(struct mntent *mp, int vfsflags, char *filteropts)
{
	int rc;

	if (fakeIt) { return 0; }

	// Mount, with fallback to read-only if necessary.

	for(;;) {
		rc = mount(mp->mnt_fsname, mp->mnt_dir, mp->mnt_type,
				vfsflags, filteropts);
		if(!rc || (vfsflags&MS_RDONLY) || (errno!=EACCES && errno!=EROFS))
			break;
		bb_error_msg("%s is write-protected, mounting read-only",
				mp->mnt_fsname);
		vfsflags |= MS_RDONLY;
	}

	// Abort entirely if permission denied.

	if (rc && errno == EPERM)
		bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);

	/* If the mount was successful, and we're maintaining an old-style
	 * mtab file by hand, add the new entry to it now. */

	if(ENABLE_FEATURE_MTAB_SUPPORT && useMtab && !rc) {
		FILE *mountTable = setmntent(bb_path_mtab_file, "a+");
		int i;

		if(!mountTable)
			bb_error_msg("No %s\n",bb_path_mtab_file);

		// Add vfs string flags

		for(i=0; mount_options[i].flags != MS_REMOUNT; i++)
			if (mount_options[i].flags > 0)
				append_mount_options(&(mp->mnt_opts),
// Shut up about the darn const.  It's not important.  I don't care.
						(char *)mount_options[i].name);

		// Remove trailing / (if any) from directory we mounted on

		i = strlen(mp->mnt_dir);
		if(i>1 && mp->mnt_dir[i-1] == '/') mp->mnt_dir[i-1] = 0;

		// Write and close.

		if(!mp->mnt_type || !*mp->mnt_type) mp->mnt_type="--bind";
		addmntent(mountTable, mp);
		endmntent(mountTable);
		if (ENABLE_FEATURE_CLEAN_UP)
			if(strcmp(mp->mnt_type,"--bind")) mp->mnt_type = 0;
	}

	return rc;
}


// Mount one directory.  Handles NFS, loopback, autobind, and filesystem type
// detection.  Returns 0 for success, nonzero for failure.

static int singlemount(struct mntent *mp)
{
	int rc = 1, vfsflags;
	char *loopFile = 0, *filteropts = 0;
	llist_t *fl = 0;
	struct stat st;

	vfsflags = parse_mount_options(mp->mnt_opts, &filteropts);

	// Treat fstype "auto" as unspecified.

	if (mp->mnt_type && !strcmp(mp->mnt_type,"auto")) mp->mnt_type = 0;

	// Might this be an NFS filesystem?

	if (ENABLE_FEATURE_MOUNT_NFS &&
		(!mp->mnt_type || !strcmp(mp->mnt_type,"nfs")) &&
		strchr(mp->mnt_fsname, ':') != NULL)
	{
		if (nfsmount(mp->mnt_fsname, mp->mnt_dir, &vfsflags, &filteropts, 1)) {
			bb_perror_msg("nfsmount failed");
			return 1;
		} else {
			// Strangely enough, nfsmount() doesn't actually mount() anything.
			mp->mnt_type = "nfs";
			rc = mount_it_now(mp, vfsflags, filteropts);
			if (ENABLE_FEATURE_CLEAN_UP) free(filteropts);
			
			return rc;
		}
	}

	// Look at the file.  (Not found isn't a failure for remount, or for
	// a synthetic filesystem like proc or sysfs.)

	if (lstat(mp->mnt_fsname, &st));
	else if (!(vfsflags & (MS_REMOUNT | MS_BIND | MS_MOVE))) {
		// Do we need to allocate a loopback device for it?

		if (ENABLE_FEATURE_MOUNT_LOOP && S_ISREG(st.st_mode)) {
			loopFile = bb_simplify_path(mp->mnt_fsname);
			mp->mnt_fsname = 0;
			switch(set_loop(&(mp->mnt_fsname), loopFile, 0)) {
				case 0:
				case 1:
					break;
				default:
					bb_error_msg( errno == EPERM || errno == EACCES
						? bb_msg_perm_denied_are_you_root
					   	: "Couldn't setup loop device");
					return errno;
			}

		// Autodetect bind mounts

		} else if (S_ISDIR(st.st_mode) && !mp->mnt_type) vfsflags |= MS_BIND;
	}

	/* If we know the fstype (or don't need to), jump straight
	 * to the actual mount. */

	if (mp->mnt_type || (vfsflags & (MS_REMOUNT | MS_BIND | MS_MOVE)))
		rc = mount_it_now(mp, vfsflags, filteropts);

	// Loop through filesystem types until mount succeeds or we run out

	else {

		/* Initialize list of block backed filesystems.  This has to be
		 * done here so that during "mount -a", mounts after /proc shows up
		 * can autodetect. */

		if (!fslist) {
			fslist = get_block_backed_filesystems();
			if (ENABLE_FEATURE_CLEAN_UP && fslist)
				atexit(delete_block_backed_filesystems);
		}

		for (fl = fslist; fl; fl = fl->link) {
			mp->mnt_type = fl->data;

			if (!(rc = mount_it_now(mp,vfsflags, filteropts))) break;

			mp->mnt_type = 0;
		}
	}

	if (ENABLE_FEATURE_CLEAN_UP) free(filteropts);

	// If mount failed, clean up loop file (if any).

	if (rc && loopFile) {
		del_loop(mp->mnt_fsname);
		if (ENABLE_FEATURE_CLEAN_UP) {
			free(loopFile);
			free(mp->mnt_fsname);
		}
	}
	return rc;
}


// Parse options, if necessary parse fstab/mtab, and call singlemount for
// each directory to be mounted.

int mount_main(int argc, char **argv)
{
	char *cmdopts = bb_xstrdup(""), *fstabname, *fstype=0, *storage_path=0;
	FILE *fstab;
	int i, opt, all = FALSE, rc = 1;
	struct mntent mtpair[2], *mtcur = mtpair;

	/* parse long options, like --bind and --move.  Note that -o option
	 * and --option are synonymous.  Yes, this means --remount,rw works. */

	for (i = opt = 0; i < argc; i++) {
		if (argv[i][0] == '-' && argv[i][1] == '-') {
			append_mount_options(&cmdopts,argv[i]+2);
		} else argv[opt++] = argv[i];
	}
	argc = opt;

	// Parse remaining options

	while ((opt = getopt(argc, argv, "o:t:rwavnf")) > 0) {
		switch (opt) {
			case 'o':
				append_mount_options(&cmdopts, optarg);
				break;
			case 't':
				fstype = optarg;
				break;
			case 'r':
				append_mount_options(&cmdopts, "ro");
				break;
			case 'w':
				append_mount_options(&cmdopts, "rw");
				break;
			case 'a':
				all = TRUE;
				break;
			case 'n':
				USE_FEATURE_MTAB_SUPPORT(useMtab = FALSE;)
				break;
			case 'f':
				USE_FEATURE_MTAB_SUPPORT(fakeIt = FALSE;)
				break;
			case 'v':
				break;		// ignore -v
			default:
				bb_show_usage();
		}
	}

	// Three or more non-option arguments?  Die with a usage message.

	if (optind-argc>2) bb_show_usage();

	// If we have no arguments, show currently mounted filesystems

	if (optind == argc) {
		if (!all) {
			FILE *mountTable = setmntent(bb_path_mtab_file, "r");

			if(!mountTable) bb_error_msg_and_die("No %s",bb_path_mtab_file);

			while (getmntent_r(mountTable,mtpair,bb_common_bufsiz1,
								sizeof(bb_common_bufsiz1)))
			{
				// Don't show rootfs.
				if (!strcmp(mtpair->mnt_fsname, "rootfs")) continue;

				if (!fstype || !strcmp(mtpair->mnt_type, fstype))
					printf("%s on %s type %s (%s)\n", mtpair->mnt_fsname,
							mtpair->mnt_dir, mtpair->mnt_type,
							mtpair->mnt_opts);
			}
			if (ENABLE_FEATURE_CLEAN_UP) endmntent(mountTable);
			return EXIT_SUCCESS;
		}
	}

	// When we have two arguments, the second is the directory and we can
	// skip looking at fstab entirely.  We can always abspath() the directory
	// argument when we get it.

	if (optind+2 == argc) {
		mtpair->mnt_fsname = argv[optind];
		mtpair->mnt_dir = argv[optind+1];
		mtpair->mnt_type = fstype;
		mtpair->mnt_opts = cmdopts;
		rc = singlemount(mtpair);
		goto clean_up;
	}

	// If we have at least one argument, it's the storage location

	if (optind < argc) storage_path = bb_simplify_path(argv[optind]);

	// Open either fstab or mtab

	if (parse_mount_options(cmdopts,0) & MS_REMOUNT)
		fstabname = (char *)bb_path_mtab_file;  // Again with the evil const.
	else fstabname="/etc/fstab";

	if (!(fstab=setmntent(fstabname,"r")))
		bb_perror_msg_and_die("Cannot read %s",fstabname);

	// Loop through entries until we find what we're looking for.

	memset(mtpair,0,sizeof(mtpair));
	for (;;) {
		struct mntent *mtnext = mtpair + (mtcur==mtpair ? 1 : 0);

		// Get next fstab entry

		if (!getmntent_r(fstab, mtcur, bb_common_bufsiz1
					+ (mtcur==mtpair ? sizeof(bb_common_bufsiz1)/2 : 0),
				sizeof(bb_common_bufsiz1)/2))
		{
			// Were we looking for something specific?

			if (optind != argc) {

				// If we didn't find anything, complain.

				if (!mtnext->mnt_fsname)
					bb_error_msg_and_die("Can't find %s in %s",
						argv[optind], fstabname);

				// Mount the last thing we found.

				mtcur = mtnext;
				mtcur->mnt_opts=bb_xstrdup(mtcur->mnt_opts);
				append_mount_options(&(mtcur->mnt_opts),cmdopts);
				rc = singlemount(mtcur);
				free(mtcur->mnt_opts);
			}
			break;
		}

		/* If we're trying to mount something specific and this isn't it,
		 * skip it.  Note we must match both the exact text in fstab (ala
		 * "proc") or a full path from root */

		if (optind != argc) {

			// Is this what we're looking for?

			if(strcmp(argv[optind],mtcur->mnt_fsname) &&
			   strcmp(storage_path,mtcur->mnt_fsname) &&
			   strcmp(argv[optind],mtcur->mnt_dir) &&
			   strcmp(storage_path,mtcur->mnt_dir)) continue;

			// Remember this entry.  Something later may have overmounted
			// it, and we want the _last_ match.

			mtcur = mtnext;

		// If we're mounting all.

		} else {

			// Do we need to match a filesystem type?
			if (fstype && strcmp(mtcur->mnt_type,fstype)) continue;

			// Skip noauto and swap anyway.

			if (parse_mount_options(mtcur->mnt_opts,0)
				& (MOUNT_NOAUTO | MOUNT_SWAP)) continue;

			// Mount this thing.

			if (singlemount(mtcur)) {
				// Don't whine about already mounted fs when mounting all.
				// Note: we should probably change return value to indicate 
				// failure, without causing a duplicate error message.
				if (errno != EBUSY) bb_perror_msg("Mounting %s on %s failed",
						mtcur->mnt_fsname, mtcur->mnt_dir);
				rc = 0;
			}
		}
	}
	if (ENABLE_FEATURE_CLEAN_UP) endmntent(fstab);

clean_up:

	if (ENABLE_FEATURE_CLEAN_UP) {
		free(storage_path);
		free(cmdopts);
	}

	if(rc)
		bb_perror_msg("Mounting %s on %s failed",
				mtcur->mnt_fsname, mtcur->mnt_dir);

	return rc;
}
