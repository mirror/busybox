/* vi: set sw=4 ts=4: */
/*
 * Mini umount implementation for busybox
 *
 * Copyright (C) 1999-2004 by Erik Andersen <andersen@codepoet.org>
 * Copyright (C) 2005 by Rob Landley <rob@landley.net>
 *
 * Licensed under GPL version 2, see file LICENSE in this tarball for details.
 */

#include "busybox.h"
#include <mntent.h>
#include <getopt.h>

#define OPTION_STRING		"flDnravd"
#define OPT_FORCE			1
#define OPT_LAZY			2
#define OPT_DONTFREELOOP	4
#define OPT_NO_MTAB			8
#define OPT_REMOUNT			16
#define OPT_ALL				(ENABLE_FEATURE_UMOUNT_ALL ? 32 : 0)

int umount_main(int argc, char **argv);
int umount_main(int argc, char **argv)
{
	int doForce;
	char path[2*PATH_MAX];
	struct mntent me;
	FILE *fp;
	int status = EXIT_SUCCESS;
	unsigned opt;
	struct mtab_list {
		char *dir;
		char *device;
		struct mtab_list *next;
	} *mtl, *m;

	/* Parse any options */

	opt = getopt32(argc, argv, OPTION_STRING);

	argc -= optind;
	argv += optind;

	doForce = MAX((opt & OPT_FORCE), (opt & OPT_LAZY));

	/* Get a list of mount points from mtab.  We read them all in now mostly
	 * for umount -a (so we don't have to worry about the list changing while
	 * we iterate over it, or about getting stuck in a loop on the same failing
	 * entry.  Notice that this also naturally reverses the list so that -a
	 * umounts the most recent entries first. */

	m = mtl = 0;

	/* If we're umounting all, then m points to the start of the list and
	 * the argument list should be empty (which will match all). */

	fp = setmntent(bb_path_mtab_file, "r");
	if (!fp) {
		if (opt & OPT_ALL)
			bb_error_msg_and_die("cannot open %s", bb_path_mtab_file);
	} else {
		while (getmntent_r(fp, &me, path, sizeof(path))) {
			m = xmalloc(sizeof(struct mtab_list));
			m->next = mtl;
			m->device = xstrdup(me.mnt_fsname);
			m->dir = xstrdup(me.mnt_dir);
			mtl = m;
		}
		endmntent(fp);
	}

	/* If we're not umounting all, we need at least one argument. */
	if (!(opt & OPT_ALL)) {
		m = 0;
		if (!argc) bb_show_usage();
	}

	// Loop through everything we're supposed to umount, and do so.
	for (;;) {
		int curstat;
		char *zapit = *argv;

		// Do we already know what to umount this time through the loop?
		if (m) safe_strncpy(path, m->dir, PATH_MAX);
		// For umount -a, end of mtab means time to exit.
		else if (opt & OPT_ALL) break;
		// Get next command line argument (and look it up in mtab list)
		else if (!argc--) break;
		else {
			argv++;
			realpath(zapit, path);
			for (m = mtl; m; m = m->next)
				if (!strcmp(path, m->dir) || !strcmp(path, m->device))
					break;
		}
		// If we couldn't find this sucker in /etc/mtab, punt by passing our
		// command line argument straight to the umount syscall.  Otherwise,
		// umount the directory even if we were given the block device.
		if (m) zapit = m->dir;

		// Let's ask the thing nicely to unmount.
		curstat = umount(zapit);

		// Force the unmount, if necessary.
		if (curstat && doForce) {
			curstat = umount2(zapit, doForce);
			if (curstat)
				bb_error_msg("forced umount of %s failed!", zapit);
		}

		// If still can't umount, maybe remount read-only?
		if (curstat && (opt & OPT_REMOUNT) && errno == EBUSY && m) {
			curstat = mount(m->device, zapit, NULL, MS_REMOUNT|MS_RDONLY, NULL);
			bb_error_msg(curstat ? "cannot remount %s read-only" :
						 "%s busy - remounted read-only", m->device);
		}

		if (curstat) {
			status = EXIT_FAILURE;
			bb_perror_msg("cannot umount %s", zapit);
		} else {
			/* De-allocate the loop device.  This ioctl should be ignored on
			 * any non-loop block devices. */
			if (ENABLE_FEATURE_MOUNT_LOOP && !(opt & OPT_DONTFREELOOP) && m)
				del_loop(m->device);
			if (ENABLE_FEATURE_MTAB_SUPPORT && !(opt & OPT_NO_MTAB) && m)
				erase_mtab(m->dir);
		}

		// Find next matching mtab entry for -a or umount /dev
		// Note this means that "umount /dev/blah" will unmount all instances
		// of /dev/blah, not just the most recent.
		while (m && (m = m->next))
			if ((opt & OPT_ALL) || !strcmp(path, m->device))
				break;
	}

	// Free mtab list if necessary

	if (ENABLE_FEATURE_CLEAN_UP) {
		while (mtl) {
			m = mtl->next;
			free(mtl->device);
			free(mtl->dir);
			free(mtl);
			mtl = m;
		}
	}

	return status;
}
