/* vi: set sw=4 ts=4: */
/*
 * Mini mount implementation for busybox
 *
 * Copyright (C) 1995, 1996 by Bruce Perens <bruce@pixar.com>.
 * Copyright (C) 1999-2003 by Erik Andersen <andersen@codepoet.org>
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
 * 3/21/1999	Charles P. Wright <cpwright@cpwright.com>
 *		searches through fstab when -a is passed
 *		will try mounting stuff with all fses when passed -t auto
 *
 * 1999-04-17	Dave Cinege...Rewrote -t auto. Fixed ro mtab.
 *
 * 1999-10-07	Erik Andersen <andersen@codepoet.org>.
 *              Rewrite of a lot of code. Removed mtab usage (I plan on
 *              putting it back as a compile-time option some time), 
 *              major adjustments to option parsing, and some serious 
 *              dieting all around.
 *
 * 1999-11-06	mtab suppport is back - andersee
 *
 * 2000-01-12   Ben Collins <bcollins@debian.org>, Borrowed utils-linux's
 *              mount to add loop support.
 *
 * 2000-04-30	Dave Cinege <dcinege@psychosis.com>
 *		Rewrote fstab while loop and lower mount section. Can now do
 *		single mounts from fstab. Can override fstab options for single
 *		mount. Common mount_one call for single mounts and 'all'. Fixed
 *		mtab updating and stale entries. Removed 'remount' default. 
 *	
 */

#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <mntent.h>
#include <ctype.h>
#include "busybox.h"

#ifdef CONFIG_NFSMOUNT
#if defined(__UCLIBC__) && ! defined(__UCLIBC_HAS_RPC__)
#error "You need to build uClibc with UCLIBC_HAS_RPC for busybox mount with NFS support to compile."
#endif
#endif

enum {
	MS_MGC_VAL = 0xc0ed0000,	/* Magic number indicatng "new" flags */
	MS_RDONLY = 1,		/* Mount read-only */
	MS_NOSUID = 2,		/* Ignore suid and sgid bits */
	MS_NODEV = 4,		/* Disallow access to device special files */
	MS_NOEXEC = 8,		/* Disallow program execution */
	MS_SYNCHRONOUS = 16,	/* Writes are synced at once */
	MS_REMOUNT = 32,	/* Alter flags of a mounted FS */
	MS_MANDLOCK = 64,	/* Allow mandatory locks on an FS */
	S_QUOTA = 128,		/* Quota initialized for file/directory/symlink */
	S_APPEND = 256,		/* Append-only file */
	S_IMMUTABLE = 512,	/* Immutable file */
	MS_NOATIME = 1024,	/* Do not update access times. */
	MS_NODIRATIME = 2048,	/* Do not update directory access times */
	MS_BIND = 4096,		/* Use the new linux 2.4.x "mount --bind" feature */
};


#if defined CONFIG_FEATURE_MOUNT_LOOP
#include <fcntl.h>
#include <sys/ioctl.h>
static int use_loop = FALSE;
#endif

extern int mount(__const char *__special_file, __const char *__dir,
				 __const char *__fstype, unsigned long int __rwflag,
				 __const void *__data);
extern int umount(__const char *__special_file);
extern int umount2(__const char *__special_file, int __flags);

extern int sysfs(int option, unsigned int fs_index, char *buf);

struct mount_options {
	const char *name;
	unsigned long and;
	unsigned long or;
};

static const struct mount_options mount_options[] = {
	{"async", ~MS_SYNCHRONOUS, 0},
	{"atime", ~0, ~MS_NOATIME},
	{"defaults", ~0, 0},
	{"noauto", ~0, 0},
	{"dev", ~MS_NODEV, 0},
	{"diratime", ~0, ~MS_NODIRATIME},
	{"exec", ~MS_NOEXEC, 0},
	{"noatime", ~0, MS_NOATIME},
	{"nodev", ~0, MS_NODEV},
	{"nodiratime", ~0, MS_NODIRATIME},
	{"noexec", ~0, MS_NOEXEC},
	{"nosuid", ~0, MS_NOSUID},
	{"remount", ~0, MS_REMOUNT},
	{"ro", ~0, MS_RDONLY},
	{"rw", ~MS_RDONLY, 0},
	{"suid", ~MS_NOSUID, 0},
	{"sync", ~0, MS_SYNCHRONOUS},
	{"bind", ~0, MS_BIND},
	{0, 0, 0}
};

static int
do_mount(char *specialfile, char *dir, char *filesystemtype, long flags,
		 void *string_flags, int useMtab, int fakeIt, char *mtab_opts,
		 int mount_all)
{
	int status = 0;

#if defined CONFIG_FEATURE_MOUNT_LOOP
	char *lofile = NULL;
#endif

	if (!fakeIt) {
#if defined CONFIG_FEATURE_MOUNT_LOOP
		if (use_loop == TRUE) {
			int loro = flags & MS_RDONLY;

			lofile = specialfile;

			specialfile = find_unused_loop_device();
			if (specialfile == NULL) {
				bb_error_msg_and_die("Could not find a spare loop device");
			}
			if (set_loop(specialfile, lofile, 0, &loro)) {
				bb_error_msg_and_die("Could not setup loop device");
			}
			if (!(flags & MS_RDONLY) && loro) {	/* loop is ro, but wanted rw */
				bb_error_msg("WARNING: loop device is read-only");
				flags |= MS_RDONLY;
			}
		}
#endif
		status = mount(specialfile, dir, filesystemtype, flags, string_flags);
		if (status < 0 && errno == EROFS) {
			bb_error_msg("%s is write-protected, mounting read-only",
					  specialfile);
			status = mount(specialfile, dir, filesystemtype, flags |=
						   MS_RDONLY, string_flags);
		}
		/* Don't whine about already mounted filesystems when mounting all. */
		if (status < 0 && errno == EBUSY && mount_all) {
			return TRUE;
		}
	}


	/* If the mount was sucessful, do anything needed, then return TRUE */
	if (status == 0 || fakeIt == TRUE) {

#if defined CONFIG_FEATURE_MTAB_SUPPORT
		if (useMtab) {
			erase_mtab(specialfile);	/* Clean any stale entries */
			write_mtab(specialfile, dir, filesystemtype, flags, mtab_opts);
		}
#endif
		return (TRUE);
	}

	/* Bummer.  mount failed.  Clean up */
#if defined CONFIG_FEATURE_MOUNT_LOOP
	if (lofile != NULL) {
		del_loop(specialfile);
	}
#endif

	if (errno == EPERM) {
		bb_error_msg_and_die(bb_msg_perm_denied_are_you_root);
	}

	return (FALSE);
}


static void paste_str(char **s1, const char *s2)
{
	*s1 = xrealloc(*s1, strlen(*s1) + strlen(s2) + 1);
	strcat(*s1, s2);
}

/* Seperate standard mount options from the nonstandard string options */
static void parse_mount_options(char *options, int *flags, char **strflags)
{
	while (options) {
		int gotone = FALSE;
		char *comma = strchr(options, ',');
		const struct mount_options *f = mount_options;

		if (comma) {
			*comma = '\0';
		}

		while (f->name != 0) {
			if (strcasecmp(f->name, options) == 0) {

				*flags &= f->and;
				*flags |= f->or;
				gotone = TRUE;
				break;
			}
			f++;
		}
#if defined CONFIG_FEATURE_MOUNT_LOOP
		if (!strcasecmp("loop", options)) {	/* loop device support */
			use_loop = TRUE;
			gotone = TRUE;
		}
#endif
		if (!gotone) {
			if (**strflags) {
				/* have previous parsed options */
				paste_str(strflags, ",");
			}
			paste_str(strflags, options);
		}
		if (comma) {
			*comma = ',';
			options = ++comma;
		} else {
			break;
		}
	}
}

static int mount_one(char *blockDevice, char *directory, char *filesystemType,
					 unsigned long flags, char *string_flags, int useMtab,
					 int fakeIt, char *mtab_opts, int whineOnErrors,
					 int mount_all)
{
	int status = 0;
	if (strcmp(filesystemType, "auto") == 0) {
		char buf[255];
		FILE *f;
		int read_proc = 0;

		f = fopen("/etc/filesystems", "r");

		if (f) {
			while (fgets(buf, sizeof(buf), f)) {
				if (*buf == '*') {
					read_proc = 1;
				} else if (*buf == '#') {
					continue;
				} else {
					filesystemType = buf;

					/* Add NULL termination to each line */
					while (*filesystemType && !isspace(*filesystemType)) {
						filesystemType++;
					}
					*filesystemType = '\0';

					filesystemType = buf;

					if (bb_strlen(filesystemType)) {
						status = do_mount(blockDevice, directory, filesystemType,
									 flags | MS_MGC_VAL, string_flags,
									 useMtab, fakeIt, mtab_opts, mount_all);
						if (status) {
							break;
						}
					}

				}
			}
			fclose(f);
		} else {
			read_proc = 1;
		}

		if (read_proc && !status) {
			
			f = bb_xfopen("/proc/filesystems", "r");

			while (fgets(buf, sizeof(buf), f) != NULL) {
				filesystemType = buf;
				if (*filesystemType == '\t') {	/* Not a nodev filesystem */

					/* Add NULL termination to each line */
					while (*filesystemType && *filesystemType != '\n') {
						filesystemType++;
					}
					*filesystemType = '\0';

					filesystemType = buf;
					filesystemType++;	/* hop past tab */

					status = do_mount(blockDevice, directory, filesystemType,
								 flags | MS_MGC_VAL, string_flags, useMtab,
								 fakeIt, mtab_opts, mount_all);
					if (status) {
						break;
					}
				}
			}
			fclose(f);
		}
	} else {
		status = do_mount(blockDevice, directory, filesystemType,
					 flags | MS_MGC_VAL, string_flags, useMtab, fakeIt,
					 mtab_opts, mount_all);
	}

	if (!status) {
		if (whineOnErrors) {
			bb_perror_msg("Mounting %s on %s failed", blockDevice, directory);
		}
		return (FALSE);
	}
	return (TRUE);
}

static void show_mounts(char *onlytype)
{
	FILE *mountTable = setmntent(bb_path_mtab_file, "r");

	if (mountTable) {
		struct mntent *m;

		while ((m = getmntent(mountTable)) != 0) {
			char *blockDevice = m->mnt_fsname;

			if (strcmp(blockDevice, "rootfs") == 0) {
				continue;
			} else if (strcmp(blockDevice, "/dev/root") == 0) {
				blockDevice = find_real_root_device_name(blockDevice);
			}
			if (!onlytype || (strcmp(m->mnt_type, onlytype) == 0)) {
				printf("%s on %s type %s (%s)\n", blockDevice, m->mnt_dir,
					   m->mnt_type, m->mnt_opts);
			}
#ifdef CONFIG_FEATURE_CLEAN_UP
			if (blockDevice != m->mnt_fsname) {
				free(blockDevice);
			}
#endif
		}
		endmntent(mountTable);
	} else {
		bb_perror_msg_and_die(bb_path_mtab_file);
	}
	exit(EXIT_SUCCESS);
}

extern int mount_main(int argc, char **argv)
{
	struct stat statbuf;
	char *string_flags = bb_xstrdup("");
	char *extra_opts;
	int flags = 0;
	char *filesystemType = "auto";
	int got_filesystemType = 0;
	char *device = xmalloc(PATH_MAX);
	char *directory = xmalloc(PATH_MAX);
	struct mntent *m = NULL;
	int all = FALSE;
	int fakeIt = FALSE;
	int useMtab = TRUE;
	int rc = EXIT_FAILURE;
	FILE *f = 0;
	int opt;

	/* Parse options */
	while ((opt = getopt(argc, argv, "o:rt:wafnv")) > 0) {
		switch (opt) {
		case 'o':
			parse_mount_options(optarg, &flags, &string_flags);
			break;
		case 'r':
			flags |= MS_RDONLY;
			break;
		case 't':
			filesystemType = optarg;
			got_filesystemType = 1;
			break;
		case 'w':
			flags &= ~MS_RDONLY;
			break;
		case 'a':
			all = TRUE;
			break;
		case 'f':
			fakeIt = TRUE;
			break;
#ifdef CONFIG_FEATURE_MTAB_SUPPORT
		case 'n':
			useMtab = FALSE;
			break;
#endif
		case 'v':
			break;		/* ignore -v */
		}
	}

	if (!all && (optind == argc)) {
		show_mounts(got_filesystemType ? filesystemType : NULL);
	}

	if (optind < argc) {
		/* if device is a filename get its real path */
		if (stat(argv[optind], &statbuf) == 0) {
			char *tmp = bb_simplify_path(argv[optind]);

			safe_strncpy(device, tmp, PATH_MAX);
		} else {
			safe_strncpy(device, argv[optind], PATH_MAX);
		}
	}

	if (optind + 1 < argc)
		directory = bb_simplify_path(argv[optind + 1]);

	if (all || optind + 1 == argc) {
		f = setmntent("/etc/fstab", "r");

		if (f == NULL)
			bb_perror_msg_and_die("\nCannot read /etc/fstab");

		while ((m = getmntent(f)) != NULL) {
			if (!all && (optind + 1 == argc)
				&& ((strcmp(device, m->mnt_fsname) != 0)
					&& (strcmp(device, m->mnt_dir) != 0))) {
				continue;
			}

			if (all && (	/* If we're mounting 'all' */
						   (strstr(m->mnt_opts, "noauto")) ||	/* and the file system isn't noauto, */
						   (strstr(m->mnt_type, "swap")))) /* and isn't swap, then mount it */
			{
				continue;
			}

			if (all || flags == 0) {	/* Allow single mount to override fstab flags */
				flags = 0;
				string_flags[0] = 0;
				parse_mount_options(m->mnt_opts, &flags, &string_flags);
			}

			strcpy(device, m->mnt_fsname);
			strcpy(directory, m->mnt_dir);
			filesystemType = bb_xstrdup(m->mnt_type);
		  singlemount:
			extra_opts = string_flags;
			rc = EXIT_SUCCESS;
#ifdef CONFIG_NFSMOUNT
			if (strchr(device, ':') != NULL) {
				filesystemType = "nfs";
				if (nfsmount
					(device, directory, &flags, &extra_opts, &string_flags,
					 1)) {
					bb_perror_msg("nfsmount failed");
					rc = EXIT_FAILURE;
				}
			}
#endif
			if (!mount_one
				(device, directory, filesystemType, flags, string_flags,
				 useMtab, fakeIt, extra_opts, TRUE, all)) {
				rc = EXIT_FAILURE;
			}
			if (!all) {
				break;
			}
		}
		if (f) {
			endmntent(f);
		}
		if (!all && f && m == NULL) {
			fprintf(stderr, "Can't find %s in /etc/fstab\n", device);
		}
		return rc;
	}

	goto singlemount;
}
