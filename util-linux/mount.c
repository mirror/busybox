/* vi: set sw=4 ts=4: */
/*
 * Mini mount implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2005 by Rob Landley <rob@landley.net>
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
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

/* Consume standard mount options (from -o options or --options).
 * Set appropriate flags and collect unrecognized ones as a comma separated
 * string to pass to kernel */ 

struct {
	const char *name;
	long flags;
} static const  mount_options[] = {
	{"loop", 0},
	{"defaults", 0},
	{"noauto", 0},
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
	{"remount", MS_REMOUNT},
	{"atime", ~MS_NOATIME},
	{"noatime", MS_NOATIME},
	{"diratime", ~MS_NODIRATIME},
	{"nodiratime", MS_NODIRATIME},
	{"bind", MS_BIND},
	{"move", MS_MOVE}
};

/* Uses the mount_options list above */
static void parse_mount_options(char *options, int *flags, char **strflags)
{
	// Loop through options
	for(;;) {
		int i;
		char *comma = strchr(options, ',');

		if(comma) *comma = 0;

		// Find this option in mount_options
		for(i = 0; i < (sizeof(mount_options) / sizeof(*mount_options)); i++) {
			if(!strcasecmp(mount_options[i].name, options)) {
				long fl = mount_options[i].flags;
				if(fl < 0) *flags &= fl;
				else *flags |= fl;
				break;
			}
		}
		// Unrecognized mount option?
		if(i == (sizeof(mount_options) / sizeof(*mount_options))) {
			// Add it to strflags, to pass on to kernel
			i = *strflags ? strlen(*strflags) : 0;
			*strflags = xrealloc(*strflags, i+strlen(options)+2);
			// Comma separated if it's not the first one
			if(i) (*strflags)[i++] = ',';
			strcpy((*strflags)+i, options);
		}
		// Advance to next option, or finish
		if(comma) {
			*comma = ',';
			options = ++comma;
		} else break;
	}
}

/* This does the work */

extern int mount_main(int argc, char **argv)
{
	char *string_flags = 0, *fsType = 0, *blockDevice = 0, *directory = 0,
		 *loopFile = 0, *buf = 0,
		 *files[] = {"/etc/filesystems", "/proc/filesystems", 0};
	int i, opt, all = FALSE, fakeIt = FALSE, allowWrite = FALSE,
	   	rc = 1, useMtab = ENABLE_FEATURE_MTAB_SUPPORT;
	int flags=0xc0ed0000;	// Needed for linux 2.2, ignored by 2.4 and 2.6.
	FILE *file = 0,*f = 0;
	char path[PATH_MAX*2];
	struct mntent m;
	struct stat statbuf;

	/* parse long options, like --bind and --move.  Note that -o option
	 * and --option are synonymous.  Yes, this means --remount,rw works. */

	for(i = opt = 0; i < argc; i++) {
		if(argv[i][0] == '-' && argv[i][1] == '-')
			parse_mount_options(argv[i]+2, &flags, &string_flags);
		else argv[opt++] = argv[i];
	}
	argc = opt;

	// Parse remaining options

	while((opt = getopt(argc, argv, "o:t:rwafnv")) > 0) {
		switch (opt) {
		case 'o':
			parse_mount_options(optarg, &flags, &string_flags);
			break;
		case 't':
			fsType = optarg;
			break;
		case 'r':
			flags |= MS_RDONLY;
			break;
		case 'w':
			allowWrite=TRUE;
			break;
		case 'a':
			all = TRUE;
			break;
		case 'f':
			fakeIt = TRUE;
			break;
		case 'n':
			useMtab = FALSE;
			break;
		case 'v':
			break;		// ignore -v
		default:
			bb_show_usage();
		}
	}

	// If we have no arguments, show currently mounted filesystems

	if(!all && (optind == argc)) {
		FILE *mountTable = setmntent(bb_path_mtab_file, "r");

		if(!mountTable) bb_perror_msg_and_die(bb_path_mtab_file);

		while (getmntent_r(mountTable,&m,path,sizeof(path))) {
			blockDevice = m.mnt_fsname;

			// Clean up display a little bit regarding root device
			if(!strcmp(blockDevice, "rootfs")) continue;
			if(!strcmp(blockDevice, "/dev/root"))
				blockDevice = find_block_device("/");

			if(!fsType || !strcmp(m.mnt_type, fsType))
				printf("%s on %s type %s (%s)\n", blockDevice, m.mnt_dir,
					   m.mnt_type, m.mnt_opts);
			if(ENABLE_FEATURE_CLEAN_UP && blockDevice != m.mnt_fsname)
				free(blockDevice);
		}
		endmntent(mountTable);
		return EXIT_SUCCESS;
	}

	/* The next argument is what to mount.  if there's an argument after that
	 * it's where to mount it.  If we're not mounting all, and we have both
	 * of these arguments, jump straight to the actual mount. */

	statbuf.st_mode=0;
	if(optind < argc)
		blockDevice = !stat(argv[optind], &statbuf) ?
		  	 	bb_simplify_path(argv[optind]) :
				(ENABLE_FEATURE_CLEAN_UP ? strdup(argv[optind]) : argv[optind]);
	if(optind+1 < argc) directory = bb_simplify_path(argv[optind+1]);

    // If we don't have to loop through fstab, skip ahead a bit.

	if(!all && optind+1!=argc) goto singlemount;

	// Loop through /etc/fstab entries to look up this entry.

	if(!(file=setmntent("/etc/fstab","r")))
		bb_perror_msg_and_die("\nCannot read /etc/fstab");
	for(;;) {

		// Get next fstab entry

		if(!getmntent_r(file,&m,path,sizeof(path))) {
			if(!all)
				bb_perror_msg("Can't find %s in /etc/fstab\n", blockDevice);
			break;
		}
		
		// If we're mounting all and all doesn't mount this one, skip it.

		if(all) {
			if(strstr(m.mnt_opts,"noauto") || strstr(m.mnt_type,"swap"))
				continue;
			flags=0;

		/* If we're mounting something specific and this isn't it, skip it.
		 * Note we must match both the exact text in fstab (ala "proc") or
		 * a full path from root */

		} else if(strcmp(blockDevice,m.mnt_fsname) &&
				  strcmp(argv[optind],m.mnt_fsname) &&
				  strcmp(blockDevice,m.mnt_dir) &&
				  strcmp(argv[optind],m.mnt_dir)) continue;

		/* Parse flags from /etc/fstab (unless this is a single mount
		 * overriding fstab -- note the "all" test above zeroed the flags,
		 * to prevent flags from previous entries affecting this one, so
		 * the only way we could get here with nonzero flags is a single
		 * mount). */

		if(!flags) {
			if(ENABLE_FEATURE_CLEAN_UP) free(string_flags);
			string_flags=NULL;
			parse_mount_options(m.mnt_opts, &flags, &string_flags);
		}

		/* Fill out remaining fields with info from mtab */

		if(ENABLE_FEATURE_CLEAN_UP) {
			free(blockDevice);
			blockDevice=strdup(m.mnt_fsname);
			free(directory);
			directory=strdup(m.mnt_dir);
		} else {
			blockDevice=m.mnt_fsname;
			directory=m.mnt_dir;
		}
		fsType=m.mnt_type;

		/* Ok, we're ready to actually mount a specific source on a specific
		 * directory now. */

singlemount:

		// If they said -w, override fstab

		if(allowWrite) flags&=~MS_RDONLY;

		// Might this be an NFS filesystem?

		if(ENABLE_FEATURE_MOUNT_NFS && (!fsType || !strcmp(fsType,"nfs")) &&
			strchr(blockDevice, ':') != NULL)
		{
			if(nfsmount(blockDevice, directory, &flags, &string_flags, 1))
				bb_perror_msg("nfsmount failed");
			else {
				rc = 0;
				fsType="nfs";
				// Strangely enough, nfsmount() doesn't actually mount()
				goto mount_it_now;
			}
		} else {
			
			// Do we need to allocate a loopback device?

			if(ENABLE_FEATURE_MOUNT_LOOP && !fakeIt && S_ISREG(statbuf.st_mode))
			{
				loopFile = blockDevice;
				blockDevice = 0;
				switch(set_loop(&blockDevice, loopFile, 0)) {
					case 0:
					case 1:
						break;
					default:
						bb_error_msg_and_die(
							errno == EPERM || errno == EACCES ?
							bb_msg_perm_denied_are_you_root :
							"Couldn't setup loop device");
						break;
				}
			}

			/* If we know the fstype (or don't need to), jump straight
			 * to the actual mount. */

			if(fsType || (flags & (MS_REMOUNT | MS_BIND | MS_MOVE)))
				goto mount_it_now;
		}
		
		// Loop through filesystem types until mount succeeds or we run out

		for(i = 0; files[i] && rc; i++) {
			f = fopen(files[i], "r");
			if(!f) continue;
			// Get next block device backed filesystem
			for(buf = 0; (buf = fsType = bb_get_chomped_line_from_file(f));
				   	free(buf))
			{
				// Skip funky entries in /proc
				if(!strncmp(buf,"nodev",5) && isspace(buf[5])) continue;
				
				while(isspace(*fsType)) fsType++;
				if(*buf=='#' || *buf=='*') continue;
				if(!*fsType) continue;
mount_it_now:
				// Okay, try to mount

				if (!fakeIt) {
					for(;;) {
						rc = mount(blockDevice, directory, fsType, flags, string_flags);
						if(!rc || (flags&MS_RDONLY) || (errno!=EACCES && errno!=EROFS))
							break;
						bb_error_msg("%s is write-protected, mounting read-only", blockDevice);
						flags|=MS_RDONLY;
					}
				}
				if(!rc || !f) break;
			}
			if(!f) break;
			fclose(f);
			// goto mount_it_now with -a can jump past the initialization
			f=0;
			if(!rc) break;
		}

		/* If the mount was successful, and we're maintaining an old-style
		 * mtab file by hand, add new entry to it now. */
		if((!rc || fakeIt) && useMtab) {
			FILE *mountTable = setmntent(bb_path_mtab_file, "a+");

			if(!mountTable) bb_perror_msg(bb_path_mtab_file);
			else {
				// Remove trailing / (if any) from directory we mounted on
				int length=strlen(directory);
				if(length>1 && directory[length-1] == '/')
					directory[length-1]=0;

				// Fill out structure (should be ok to re-use existing one).
				m.mnt_fsname=blockDevice;
				m.mnt_dir=directory;
				m.mnt_type=fsType ? : "--bind";
				m.mnt_opts=string_flags ? :
					((flags & MS_RDONLY) ? "ro" : "rw");
				m.mnt_freq = 0;
				m.mnt_passno = 0;

				// Write and close
				addmntent(mountTable, &m);
				endmntent(mountTable);
			}
		} else {
			// Mount failed.  Clean up
			if(loopFile) {
				del_loop(blockDevice);
				if(ENABLE_FEATURE_CLEAN_UP) free(loopFile);
			}
			// Don't whine about already mounted fs when mounting all.
			if(rc<0 && errno == EBUSY && all) rc = 0;
			else if (errno == EPERM)
				bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);
		}
		// We couldn't free this earlier becase fsType could be in buf.
		if(ENABLE_FEATURE_CLEAN_UP) free(buf);
		if(!all) break;
	}

	if(file) endmntent(file);
	if(rc) bb_perror_msg("Mounting %s on %s failed", blockDevice, directory);
	if(ENABLE_FEATURE_CLEAN_UP) {
		free(blockDevice);
		free(directory);
	}

	return rc;
}
