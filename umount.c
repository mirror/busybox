/* vi: set sw=4 ts=4: */
/*
 * Mini umount implementation for busybox
 *
 *
 * Copyright (C) 1999 by Lineo, inc.
 * Written by Erik Andersen <andersen@lineo.com>, <andersee@debian.org>
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

#include "internal.h"
#include <stdio.h>
#include <sys/mount.h>
#include <mntent.h>
#include <fstab.h>
#include <errno.h>


static const char umount_usage[] =
	"umount [flags] filesystem|directory\n\n"
	"Flags:\n" "\t-a:\tUnmount all file systems"
#ifdef BB_MTAB
	" in /etc/mtab\n\t-n:\tDon't erase /etc/mtab entries\n"
#else
	"\n"
#endif
#ifdef BB_FEATURE_REMOUNT
	"\t-r:\tTry to remount devices as read-only if mount is busy\n"
#endif
#if defined BB_FEATURE_MOUNT_LOOP
	"\t-f:\tDo not free loop device (if a loop device has been used)\n"
#endif
;

struct _mtab_entry_t {
	char *device;
	char *mountpt;
	struct _mtab_entry_t *next;
};

static struct _mtab_entry_t *mtab_cache = NULL;



#if defined BB_FEATURE_MOUNT_LOOP
static int freeLoop = TRUE;
#endif
static int useMtab = TRUE;
static int umountAll = FALSE;
#if defined BB_FEATURE_REMOUNT
static int doRemount = FALSE;
#endif
extern const char mtab_file[];	/* Defined in utility.c */



/* These functions are here because the getmntent functions do not appear
 * to be re-entrant, which leads to all sorts of problems when we try to
 * use them recursively - randolph
 *
 * TODO: Perhaps switch to using Glibc's getmntent_r
 *        -Erik
 */
void mtab_read(void)
{
	struct _mtab_entry_t *entry = NULL;
	struct mntent *e;
	FILE *fp;

	if (mtab_cache != NULL)
		return;

	if ((fp = setmntent(mtab_file, "r")) == NULL) {
		fprintf(stderr, "Cannot open %s\n", mtab_file);
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

char *mtab_getinfo(const char *match, const char which)
{
	struct _mtab_entry_t *cur = mtab_cache;

	while (cur) {
		if (strcmp(cur->mountpt, match) == 0 ||
			strcmp(cur->device, match) == 0) {
			if (which == MTAB_GETMOUNTPT) {
				return cur->mountpt;
			} else {
#if !defined BB_MTAB
				if (strcmp(cur->device, "/dev/root") == 0) {
					struct fstab *fstabItem;

					fstabItem = getfsfile("/");
					if (fstabItem != NULL)
						return fstabItem->fs_spec;
				}
#endif
				return cur->device;
			}
		}
		cur = cur->next;
	}
	return NULL;
}

char *mtab_first(void **iter)
{
	struct _mtab_entry_t *mtab_iter;

	if (!iter)
		return NULL;
	mtab_iter = mtab_cache;
	*iter = (void *) mtab_iter;
	return mtab_next(iter);
}

char *mtab_next(void **iter)
{
	char *mp;

	if (iter == NULL || *iter == NULL)
		return NULL;
	mp = ((struct _mtab_entry_t *) (*iter))->mountpt;
	*iter = (void *) ((struct _mtab_entry_t *) (*iter))->next;
	return mp;
}

void mtab_free(void)
{
	struct _mtab_entry_t *this, *next;

	this = mtab_cache;
	while (this) {
		next = this->next;
		if (this->device)
			free(this->device);
		if (this->mountpt)
			free(this->mountpt);
		free(this);
		this = next;
	}
}

static int do_umount(const char *name, int useMtab)
{
	int status;
	char *blockDevice = mtab_getinfo(name, MTAB_GETDEVICE);

	if (blockDevice && strcmp(blockDevice, name) == 0)
		name = mtab_getinfo(blockDevice, MTAB_GETMOUNTPT);

	status = umount(name);

#if defined BB_FEATURE_MOUNT_LOOP
	if (freeLoop == TRUE && blockDevice != NULL && !strncmp("/dev/loop", blockDevice, 9))
		/* this was a loop device, delete it */
		del_loop(blockDevice);
#endif
#if defined BB_FEATURE_REMOUNT
	if (status != 0 && doRemount == TRUE && errno == EBUSY) {
		status = mount(blockDevice, name, NULL,
					   MS_MGC_VAL | MS_REMOUNT | MS_RDONLY, NULL);
		if (status == 0) {
			fprintf(stderr, "umount: %s busy - remounted read-only\n",
					blockDevice);
			/* TODO: update mtab if BB_MTAB is defined */
		} else {
			fprintf(stderr, "umount: Cannot remount %s read-only\n",
					blockDevice);
		}
	}
#endif
	if (status == 0) {
#if defined BB_MTAB
		if (useMtab == TRUE)
			erase_mtab(name);
#endif
		return (TRUE);
	}
	return (FALSE);
}

static int umount_all(int useMtab)
{
	int status = TRUE;
	char *mountpt;
	void *iter;

	for (mountpt = mtab_first(&iter); mountpt; mountpt = mtab_next(&iter)) {
		/* Never umount /proc on a umount -a */
		if (strstr(mountpt, "proc")!= NULL)
			continue;
		status = do_umount(mountpt, useMtab);
		if (status != 0) {
			/* Don't bother retrying the umount on busy devices */
			if (errno == EBUSY) {
				perror(mountpt);
				continue;
			}
			status = do_umount(mountpt, useMtab);
			if (status != 0) {
				printf("Couldn't umount %s on %s: %s\n",
					   mountpt, mtab_getinfo(mountpt, MTAB_GETDEVICE),
					   strerror(errno));
			}
		}
	}
	return (status);
}

extern int umount_main(int argc, char **argv)
{
	if (argc < 2) {
		usage(umount_usage);
	}

	/* Parse any options */
	while (--argc > 0 && **(++argv) == '-') {
		while (*++(*argv))
			switch (**argv) {
			case 'a':
				umountAll = TRUE;
				break;
#if defined BB_FEATURE_MOUNT_LOOP
			case 'f':
				freeLoop = FALSE;
				break;
#endif
#ifdef BB_MTAB
			case 'n':
				useMtab = FALSE;
				break;
#endif
#ifdef BB_FEATURE_REMOUNT
			case 'r':
				doRemount = TRUE;
				break;
#endif
			default:
				usage(umount_usage);
			}
	}

	mtab_read();
	if (umountAll == TRUE) {
		exit(umount_all(useMtab));
	}
	if (do_umount(*argv, useMtab) == 0)
		exit(TRUE);
	else {
		perror("umount");
		exit(FALSE);
	}
}

