/* vi: set sw=4 ts=4: */
/*
 * Mini umount implementation for busybox
 *
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
 */

#include <limits.h>
#include <stdio.h>
#include <mntent.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include "busybox.h"

/* Teach libc5 about realpath -- it includes it but the 
 * prototype is missing... */
#if (__GLIBC__ <= 2) && (__GLIBC_MINOR__ < 1)
extern char *realpath(const char *path, char *resolved_path);
#endif

static const int MNT_FORCE = 1;
static const int MS_MGC_VAL = 0xc0ed0000; /* Magic number indicatng "new" flags */
static const int MS_REMOUNT = 32;	/* Alter flags of a mounted FS.  */
static const int MS_RDONLY = 1;	/* Mount read-only.  */

extern int mount (__const char *__special_file, __const char *__dir,
			__const char *__fstype, unsigned long int __rwflag,
			__const void *__data);
extern int umount (__const char *__special_file);
extern int umount2 (__const char *__special_file, int __flags);

struct _mtab_entry_t {
	char *device;
	char *mountpt;
	struct _mtab_entry_t *next;
};

static struct _mtab_entry_t *mtab_cache = NULL;



#if defined CONFIG_FEATURE_MOUNT_FORCE
static int doForce = FALSE;
#endif
#if defined CONFIG_FEATURE_MOUNT_LOOP
static int freeLoop = TRUE;
#endif
#if defined CONFIG_FEATURE_MTAB_SUPPORT
static int useMtab = TRUE;
#endif
static int umountAll = FALSE;
static int doRemount = FALSE;



/* These functions are here because the getmntent functions do not appear
 * to be re-entrant, which leads to all sorts of problems when we try to
 * use them recursively - randolph
 *
 * TODO: Perhaps switch to using Glibc's getmntent_r
 *        -Erik
 */
static void mtab_read(void)
{
	struct _mtab_entry_t *entry = NULL;
	struct mntent *e;
	FILE *fp;

	if (mtab_cache != NULL)
		return;

	if ((fp = setmntent(bb_path_mtab_file, "r")) == NULL) {
		bb_error_msg("Cannot open %s", bb_path_mtab_file);
		return;
	}
	while ((e = getmntent(fp))) {
		entry = xmalloc(sizeof(struct _mtab_entry_t));
		entry->device = strdup(e->mnt_fsname);
		entry->mountpt = strdup(e->mnt_dir);
		entry->next = mtab_cache;
		mtab_cache = entry;
	}
	endmntent(fp);
}

static char *mtab_getinfo(const char *match, const char which)
{
	struct _mtab_entry_t *cur = mtab_cache;

	while (cur) {
		if (strcmp(cur->mountpt, match) == 0 ||
			strcmp(cur->device, match) == 0) {
			if (which == MTAB_GETMOUNTPT) {
				return cur->mountpt;
			} else {
#if !defined CONFIG_FEATURE_MTAB_SUPPORT
				if (strcmp(cur->device, "rootfs") == 0) {
					continue;
				} else if (strcmp(cur->device, "/dev/root") == 0) {
					/* Adjusts device to be the real root device,
					 * or leaves device alone if it can't find it */
					cur->device = find_real_root_device_name(cur->device);
				}
#endif
				return cur->device;
			}
		}
		cur = cur->next;
	}
	return NULL;
}

static char *mtab_next(void **iter)
{
	char *mp;

	if (iter == NULL || *iter == NULL)
		return NULL;
	mp = ((struct _mtab_entry_t *) (*iter))->mountpt;
	*iter = (void *) ((struct _mtab_entry_t *) (*iter))->next;
	return mp;
}

static char *mtab_first(void **iter)
{
	struct _mtab_entry_t *mtab_iter;

	if (!iter)
		return NULL;
	mtab_iter = mtab_cache;
	*iter = (void *) mtab_iter;
	return mtab_next(iter);
}

/* Don't bother to clean up, since exit() does that 
 * automagically, so we can save a few bytes */
#ifdef CONFIG_FEATURE_CLEAN_UP
static void mtab_free(void)
{
	struct _mtab_entry_t *this, *next;

	this = mtab_cache;
	while (this) {
		next = this->next;
		free(this->device);
		free(this->mountpt);
		free(this);
		this = next;
	}
}
#endif

static int do_umount(const char *name)
{
	int status;
	char *blockDevice = mtab_getinfo(name, MTAB_GETDEVICE);

	if (blockDevice && strcmp(blockDevice, name) == 0)
		name = mtab_getinfo(blockDevice, MTAB_GETMOUNTPT);

	status = umount(name);

#if defined CONFIG_FEATURE_MOUNT_LOOP
	if (freeLoop && blockDevice != NULL && !strncmp("/dev/loop", blockDevice, 9))
		/* this was a loop device, delete it */
		del_loop(blockDevice);
#endif
#if defined CONFIG_FEATURE_MOUNT_FORCE
	if (status != 0 && doForce) {
		status = umount2(blockDevice, MNT_FORCE);
		if (status != 0) {
			bb_error_msg_and_die("forced umount of %s failed!", blockDevice);
		}
	}
#endif
	if (status != 0 && doRemount && errno == EBUSY) {
		status = mount(blockDevice, name, NULL,
					   MS_MGC_VAL | MS_REMOUNT | MS_RDONLY, NULL);
		if (status == 0) {
			bb_error_msg("%s busy - remounted read-only", blockDevice);
		} else {
			bb_error_msg("Cannot remount %s read-only", blockDevice);
		}
	}
	if (status == 0) {
#if defined CONFIG_FEATURE_MTAB_SUPPORT
		if (useMtab)
			erase_mtab(name);
#endif
		return (TRUE);
	}
	return (FALSE);
}

static int umount_all(void)
{
	int status = TRUE;
	char *mountpt;
	void *iter;

	for (mountpt = mtab_first(&iter); mountpt; mountpt = mtab_next(&iter)) {
		/* Never umount /proc on a umount -a */
		if (strstr(mountpt, "proc")!= NULL)
			continue;
		if (!do_umount(mountpt)) {
			/* Don't bother retrying the umount on busy devices */
			if (errno == EBUSY) {
				bb_perror_msg("%s", mountpt);
				status = FALSE;
				continue;
			}
			if (!do_umount(mountpt)) {
				printf("Couldn't umount %s on %s: %s\n",
					   mountpt, mtab_getinfo(mountpt, MTAB_GETDEVICE),
					   strerror(errno));
				status = FALSE;
			}
		}
	}
	return (status);
}

extern int umount_main(int argc, char **argv)
{
	char path[PATH_MAX], result = 0;

	if (argc < 2) {
		bb_show_usage();
	}
#ifdef CONFIG_FEATURE_CLEAN_UP
	atexit(mtab_free);
#endif

	/* Parse any options */
	while (--argc > 0 && **(++argv) == '-') {
		while (*++(*argv))
			switch (**argv) {
			case 'a':
				umountAll = TRUE;
				break;
#if defined CONFIG_FEATURE_MOUNT_LOOP
			case 'l':
				freeLoop = FALSE;
				break;
#endif
#ifdef CONFIG_FEATURE_MTAB_SUPPORT
			case 'n':
				useMtab = FALSE;
				break;
#endif
#ifdef CONFIG_FEATURE_MOUNT_FORCE
			case 'f':
				doForce = TRUE;
				break;
#endif
			case 'r':
				doRemount = TRUE;
				break;
			case 'v':
				break; /* ignore -v */
			default:
				bb_show_usage();
			}
	}

	mtab_read();
	if (umountAll) {
		if (umount_all())
			return EXIT_SUCCESS;
		else
			return EXIT_FAILURE;
	}

	do {
		if (realpath(*argv, path) != NULL)
			if (do_umount(path))
				continue;
		bb_perror_msg("%s", path);
		result++;
	} while (--argc > 0 && ++argv);
	return result;
}
